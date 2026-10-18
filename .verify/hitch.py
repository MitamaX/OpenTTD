import argparse
import statistics
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path

TOP_LEVEL = ("tick.vidlock", "tick.lock", "tick.locked", "tick.vidunlock", "frame", "tick.swap", "tick.sleep")
CLOCK_COUNTERS = ("tick", "day", "month")
WARMUP_SECONDS = 20
HITCH_FACTOR = 1.5
SPIKE_MARGIN_MS = 3.0
LONG_FRAME_MS = 33.3
WORST_SHOWN = 12
EXCESS_SHOWN = 3
COUNTERS_SHOWN = 3
GAPS_SHOWN = 6
RUN_JOIN_FRAMES = 2
MIN_SPIKES_FOR_GAPS = 3


@dataclass
class Frame:
    number: int
    interval: float
    cpu: dict = field(default_factory=dict)
    gpu: dict = field(default_factory=dict)
    counters: dict = field(default_factory=dict)

    def top_level_sum(self):
        return sum(self.cpu.get(name, 0.0) for name in TOP_LEVEL)


def parse_line(line):
    fields = line.split()
    frame = Frame(int(fields[0]), float(fields[1]))
    for item in fields[2:]:
        name, value = item.split("=", 1)
        if name.startswith("#"):
            frame.counters[name[1:]] = int(value)
            continue
        cpu, _, gpu = value.partition("/")
        frame.cpu[name] = float(cpu)
        if gpu:
            frame.gpu[name] = float(gpu)
    return frame


def load(path, refresh):
    lines = Path(path).read_text().splitlines()
    frames = [parse_line(line) for line in lines if line.strip()]
    warmup = refresh * WARMUP_SECONDS
    return [frame for frame in frames if frame.number >= warmup]


def percentile(values, rank):
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(rank * len(ordered)))] if ordered else 0.0


def medians(frames, attribute):
    names = {name for frame in frames for name in getattr(frame, attribute)}
    return {name: statistics.median(getattr(frame, attribute).get(name, 0) for frame in frames) for name in names}


class Timeline:
    def __init__(self, path, refresh):
        self.path = Path(path)
        self.refresh = refresh
        self.frames = load(path, refresh)
        self.by_number = {frame.number: frame for frame in self.frames}
        self.intervals = [frame.interval for frame in self.frames]
        self.median_interval = statistics.median(self.intervals)
        self.threshold = self.median_interval * HITCH_FACTOR
        self.cpu_medians = medians(self.frames, "cpu")
        self.gpu_medians = medians(self.frames, "gpu")
        self.counter_medians = medians(self.frames, "counters")
        self.hitches = [frame for frame in self.frames if frame.interval > self.threshold]

    def frame_ms(self, frames):
        return frames * 1000.0 / self.refresh

    def day_changes(self):
        return [later for earlier, later in zip(self.frames, self.frames[1:]) if earlier.counters.get("day") != later.counters.get("day")]

    def summary(self):
        count = len(self.frames)
        day_intervals = [frame.interval for frame in self.day_changes()]
        return {
            "frames": count,
            "median": self.median_interval,
            "hitches": len(self.hitches),
            "ratio": len(self.hitches) / count if count else 0.0,
            "long": sum(1 for value in self.intervals if value > LONG_FRAME_MS),
            "p99": percentile(self.intervals, 0.99),
            "p999": percentile(self.intervals, 0.999),
            "max": max(self.intervals),
            "day_med": statistics.median(day_intervals) if day_intervals else 0.0,
            "gpu_frame": self.gpu_medians.get("frame", 0.0),
            "cpu_frame": self.cpu_medians.get("frame", 0.0),
            "cpu_locked": self.cpu_medians.get("tick.locked", 0.0),
        }

    def excess(self, frame):
        rises = [(frame.cpu[name] - self.cpu_medians.get(name, 0.0), name) for name in frame.cpu]
        return sorted(rises, reverse=True)[:EXCESS_SHOWN]

    def raised_counters(self, frame):
        rises = [(value - self.counter_medians.get(name, 0), name, value) for name, value in frame.counters.items() if name not in CLOCK_COUNTERS]
        return [(name, value) for rise, name, value in sorted(rises, reverse=True)[:COUNTERS_SHOWN] if rise > 0]

    def clock_change(self, frame):
        before = self.by_number.get(frame.number - 1)
        if before is None:
            return ""
        changed = [name for name in CLOCK_COUNTERS if name != "tick" and before.counters.get(name) != frame.counters.get(name)]
        ticks = frame.counters.get("tick", 0) - before.counters.get("tick", 0)
        return f"ticks+{ticks}" + "".join(f" {name}!" for name in changed)

    def describe_frame(self, frame, mark):
        top = " ".join(f"{name.split('.')[-1]}={frame.cpu.get(name, 0.0):.1f}" for name in TOP_LEVEL)
        unexplained = frame.interval - frame.top_level_sum()
        rises = ", ".join(f"{name}+{rise:.1f}" for rise, name in self.excess(frame))
        counters = ", ".join(f"#{name}={value}" for name, value in self.raised_counters(frame))
        gpu = frame.gpu.get("frame", 0.0)
        return f"{mark}{frame.number:>7} {frame.interval:6.1f}ms gpu={gpu:4.1f} {top} rest={unexplained:.1f} | {rises} | {counters} | {self.clock_change(frame)}"

    def worst(self):
        lines = []
        for frame in sorted(self.hitches, key=lambda hitch: -hitch.interval)[:WORST_SHOWN]:
            for offset in (-1, 0, 1):
                neighbour = self.by_number.get(frame.number + offset)
                if neighbour is not None:
                    lines.append(self.describe_frame(neighbour, "*" if offset == 0 else " "))
            lines.append("")
        return lines

    def blame(self):
        totals = Counter()
        leads = Counter()
        for frame in self.hitches:
            rises = self.excess(frame)
            for rise, name in rises:
                totals[name] += max(rise, 0.0)
            if rises:
                leads[rises[0][1]] += 1
        return [(name, leads[name], totals[name]) for name, _ in totals.most_common()]

    def spikes(self, name):
        floor = self.cpu_medians.get(name, 0.0) + SPIKE_MARGIN_MS
        return [frame.number for frame in self.frames if frame.cpu.get(name, 0.0) > floor]

    def periodicity(self):
        rows = []
        for name in sorted(self.cpu_medians):
            numbers = self.spikes(name)
            if not numbers:
                continue
            runs = runs_of(numbers)
            starts = [run[0] for run in runs]
            gaps = [later - earlier for earlier, later in zip(starts, starts[1:])]
            peaks = Counter(gaps).most_common(GAPS_SHOWN) if len(starts) >= MIN_SPIKES_FOR_GAPS else []
            longest = max(len(run) for run in runs)
            median_gap = statistics.median(gaps) if gaps else 0
            rows.append((name, len(numbers), len(runs), longest, median_gap, peaks))
        return rows

    def report(self):
        s = self.summary()
        lines = [
            f"# {self.path}",
            f"frames {s['frames']} median {s['median']:.2f}ms threshold {self.threshold:.2f}ms",
            f"hitches {s['hitches']} ({s['ratio'] * 100:.2f}%) over33ms {s['long']} p99 {s['p99']:.1f} p99.9 {s['p999']:.1f} max {s['max']:.1f} day-change median {s['day_med']:.1f}",
            f"medians gpu.frame {s['gpu_frame']:.2f} cpu.frame {s['cpu_frame']:.2f} tick.locked {s['cpu_locked']:.2f}",
            "",
            "## blame over hitch frames: section, frames led, summed excess ms",
        ]
        lines += [f"{name:<28}{led:>6}{total:>10.1f}" for name, led, total in self.blame()]
        lines += ["", f"## spikes over median+{SPIKE_MARGIN_MS:.0f}ms: section, frames, runs, longest run, median gap frames (ms), common gaps"]
        for name, count, runs, longest, median_gap, peaks in self.periodicity():
            common = " ".join(f"{gap}x{times}" for gap, times in peaks)
            lines.append(f"{name:<28}{count:>6}{runs:>6}{longest:>5}{median_gap:>8.0f} ({self.frame_ms(median_gap):7.0f}) {common}")
        lines += ["", "## worst hitch frames with neighbours"]
        lines += self.worst()
        return "\n".join(lines)


def runs_of(numbers):
    runs = [[numbers[0]]]
    for number in numbers[1:]:
        if number - runs[-1][-1] <= RUN_JOIN_FRAMES:
            runs[-1].append(number)
        else:
            runs.append([number])
    return runs


def compare(timelines):
    keys = ("frames", "median", "hitches", "ratio", "long", "p99", "p999", "max", "day_med", "gpu_frame", "cpu_frame", "cpu_locked")
    header = f"{'run':<40}" + "".join(f"{key:>11}" for key in keys)
    rows = [header]
    for timeline in timelines:
        s = timeline.summary()
        cells = "".join(f"{s[key] * 100:>10.2f}%" if key == "ratio" else f"{s[key]:>11.2f}" if isinstance(s[key], float) else f"{s[key]:>11}" for key in keys)
        rows.append(f"{timeline.path.parent.name:<40}{cells}")
    return "\n".join(rows)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("timelines", nargs="+")
    parser.add_argument("--refresh", type=int, default=120)
    parser.add_argument("--compare", action="store_true")
    args = parser.parse_args()
    timelines = [Timeline(path, args.refresh) for path in args.timelines]
    print(compare(timelines) if args.compare else "\n\n".join(timeline.report() for timeline in timelines))


if __name__ == "__main__":
    main()

import pathlib
import sys

COLUMNS = ("frame", "frame_p90", "draw.terrain", "draw.structures", "lay.structures", "draw.forest", "lay.forest", "post")


def blocks(path):
    result = {}
    name = None
    for line in pathlib.Path(path).read_text().splitlines():
        if line.startswith("# "):
            name = pathlib.Path(line[2:].strip()).stem
            result[name] = {}
            continue
        fields = line.split()
        if name is None or len(fields) < 7 or fields[0] == "section":
            continue
        result[name][fields[0]] = (float(fields[1]), float(fields[2]))
    return result


def row(sections):
    frame = sections.get("frame", (0.0, 0.0))
    post = sum(value[0] for key, value in sections.items() if key.startswith("post."))
    cells = {
        "frame": frame[0],
        "frame_p90": frame[1],
        "post": post,
    }
    for key in COLUMNS:
        if key not in cells:
            cells[key] = sections.get(key, (0.0, 0.0))[0]
    return cells


def main():
    print(f"{'run':<34}" + "".join(f"{column:>16}" for column in COLUMNS))
    for folder in sys.argv[1:]:
        profile = pathlib.Path(folder) / "profile.txt"
        if not profile.exists():
            continue
        for shot, sections in blocks(profile).items():
            if shot == "warm":
                continue
            cells = row(sections)
            label = f"{pathlib.Path(folder).name}/{shot}"
            print(f"{label[-34:]:<34}" + "".join(f"{cells[column]:>16.2f}" for column in COLUMNS))


if __name__ == "__main__":
    main()

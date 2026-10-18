import sys, struct, zlib

LUMA = (0.2126, 0.7152, 0.0722)
BOX_RADII = (1, 4, 13)
MARGIN = 30


def load(path):
    data = open(path, 'rb').read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    pos = 8; idat = b''; w = h = 0
    while pos < len(data):
        n = struct.unpack('>I', data[pos:pos+4])[0]; t = data[pos+4:pos+8]; body = data[pos+8:pos+8+n]; pos += 12 + n
        if t == b'IHDR': w, h, depth, ctype = struct.unpack('>IIBB', body[:10])
        elif t == b'IDAT': idat += body
    bpp = {2: 3, 6: 4}[ctype]
    raw = zlib.decompress(idat); stride = w * bpp; rows = []; prev = bytearray(stride); i = 0
    for y in range(h):
        f = raw[i]; i += 1; line = bytearray(raw[i:i+stride]); i += stride
        for x in range(stride):
            a = line[x-bpp] if x >= bpp else 0; b = prev[x]; c = prev[x-bpp] if x >= bpp else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c; pa, pb, pc = abs(p-a), abs(p-b), abs(p-c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line); prev = line
    return w, h, bpp, rows


def luminance_window(path, x, y, bw, bh):
    w, h, bpp, rows = load(path)
    x0, y0 = max(0, x - MARGIN), max(0, y - MARGIN)
    x1, y1 = min(w, x + bw + MARGIN), min(h, y + bh + MARGIN)
    lum = [[(LUMA[0] * r[i] + LUMA[1] * r[i+1] + LUMA[2] * r[i+2]) / 255.0 for i in range(x0 * bpp, x1 * bpp, bpp)] for r in rows[y0:y1]]
    return lum, x - x0, y - y0, x0, y0


def integral(lum):
    h, w = len(lum), len(lum[0])
    table = [[0.0] * (w + 1) for _ in range(h + 1)]
    for j in range(h):
        run = 0.0; row = lum[j]; up = table[j]; cur = table[j + 1]
        for i in range(w):
            run += row[i]; cur[i + 1] = up[i + 1] + run
    return table


def box_mean(table, w, h, i, j, r):
    i0, i1, j0, j1 = max(0, i - r), min(w, i + r + 1), max(0, j - r), min(h, j + r + 1)
    total = table[j1][i1] - table[j0][i1] - table[j1][i0] + table[j0][i0]
    return total / ((i1 - i0) * (j1 - j0))


def band_contrasts(path, x, y, bw, bh):
    lum, ox, oy, _, _ = luminance_window(path, x, y, bw, bh)
    h, w = len(lum), len(lum[0])
    table = integral(lum)
    sums = [[0.0, 0.0] for _ in range(3)]; mean_total = 0.0; count = 0
    for j in range(oy, min(h, oy + bh)):
        for i in range(ox, min(w, ox + bw)):
            m1, m4, m13 = (box_mean(table, w, h, i, j, r) for r in BOX_RADII)
            for k, v in enumerate((lum[j][i] - m1, m1 - m4, m4 - m13)):
                sums[k][0] += v; sums[k][1] += v * v
            mean_total += m1; count += 1
    mean = mean_total / count
    return [((s2 / count - (s1 / count) ** 2) ** 0.5) / mean for s1, s2 in sums]


def flicker(path_a, path_b, x, y, bw, bh):
    a = luminance_window(path_a, x, y, bw, bh); b = luminance_window(path_b, x, y, bw, bh)
    (la, ox, oy, _, _), (lb, _, _, _, _) = a, b
    total = 0.0
    for j in range(oy, oy + bh):
        for i in range(ox, ox + bw):
            total += abs(la[j][i] - lb[j][i])
    return total / (bw * bh)


def main(args):
    if args[0] == '--flicker':
        a, b, x, y, bw, bh = args[1], args[2], *map(int, args[3:7])
        print(f'flicker {flicker(a, b, x, y, bw, bh):.5f}')
        return
    for spec in args:
        path, x, y, bw, bh = spec.split(',')
        c1, c3, c9 = band_contrasts(path, int(x), int(y), int(bw), int(bh))
        print(f'{path.split(chr(92))[-1].split("/")[-1]} {x},{y},{bw},{bh}  1px {c1:.4f}  3-9px {c3:.4f}  9-27px {c9:.4f}  1/3-9 {c1/c3:.2f}  9-27/3-9 {c9/c3:.2f}')


if __name__ == '__main__':
    main(sys.argv[1:])

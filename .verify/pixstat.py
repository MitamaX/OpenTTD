import sys, struct, zlib
def load(path):
    data = open(path, 'rb').read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n'
    pos = 8; idat = b''; w = h = 0
    while pos < len(data):
        n = struct.unpack('>I', data[pos:pos+4])[0]; t = data[pos+4:pos+8]; body = data[pos+8:pos+8+n]; pos += 12 + n
        if t == b'IHDR': w, h, depth, ctype = struct.unpack('>IIBB', body[:10])
        elif t == b'IDAT': idat += body
    bpp = {2: 3, 6: 4}[ctype]
    raw = zlib.decompress(idat); stride = w * bpp; out = bytearray(h * stride); prev = bytearray(stride); i = 0
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
        out[y*stride:(y+1)*stride] = line; prev = line
    return w, h, bpp, out
w, h, bpp, a = load(sys.argv[1]); _, _, _, b = load(sys.argv[2])
hist = {}
for i in range(0, len(a), bpp):
    d = max(abs(a[i]-b[i]), abs(a[i+1]-b[i+1]), abs(a[i+2]-b[i+2]))
    hist[d] = hist.get(d, 0) + 1
tot = w * h
print(' '.join(f'{k}:{v*100/tot:.3f}%' for k, v in sorted(hist.items())[:12]), 'max', max(hist))

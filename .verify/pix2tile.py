import math, sys
W, H, FOV = 1600.0, 900.0, 40.0
SQ = math.sqrt(0.5)
NORTH = (-SQ, -SQ)
EAST = (-SQ, SQ)

def bearing(deg):
    r = math.radians(deg)
    return (NORTH[0] * math.cos(r) + EAST[0] * math.sin(r), NORTH[1] * math.cos(r) + EAST[1] * math.sin(r))

def tile_at(fx, fy, ppt, yaw, pitch, px, py):
    focal = H / (2 * math.tan(math.radians(FOV / 2)))
    dist = focal / ppt
    p = math.radians(pitch)
    f = bearing(yaw)
    r = bearing(yaw + 90)
    eye = (fx - f[0] * dist * math.cos(p), fy - f[1] * dist * math.cos(p), dist * math.sin(p))
    t = math.tan(math.radians(FOV / 2))
    u = (px - W / 2) / (W / 2) * t * (W / H)
    v = (H / 2 - py) / (H / 2) * t
    dirh = (f[0] * math.cos(p), f[1] * math.cos(p))
    dirz = -math.sin(p)
    upv = (f[0] * math.sin(p), f[1] * math.sin(p), math.cos(p))
    ray = (dirh[0] + u * r[0] + v * upv[0], dirh[1] + u * r[1] + v * upv[1], dirz + v * upv[2])
    s = -eye[2] / ray[2]
    return (eye[0] + ray[0] * s, eye[1] + ray[1] * s)

if __name__ == "__main__":
    fx, fy, ppt, yaw, pitch = map(float, sys.argv[1:6])
    pts = list(map(float, sys.argv[6:]))
    for i in range(0, len(pts), 2):
        x, y = tile_at(fx, fy, ppt, yaw, pitch, pts[i], pts[i + 1])
        print(f"px({pts[i]:.0f},{pts[i+1]:.0f}) -> tile ({x:.1f},{y:.1f})")

/* A cheap hash of a lattice point, enough to scatter value noise. */
float LatticeHash(ivec2 cell)
{
	uint h = uint(cell.x) * 0x8DA6B343u ^ uint(cell.y) * 0xD8163841u;
	h = (h ^ (h >> 15u)) * 0x2C1B3C6Du;
	h ^= h >> 12u;
	return float(h) * (1.0 / 4294967296.0);
}

float Noise(vec2 p)
{
	ivec2 cell = ivec2(floor(p));
	vec2 f = fract(p);
	vec2 s = f * f * (3.0 - 2.0 * f);
	float a = LatticeHash(cell);
	float b = LatticeHash(cell + ivec2(1, 0));
	float c = LatticeHash(cell + ivec2(0, 1));
	float d = LatticeHash(cell + ivec2(1, 1));
	return mix(mix(a, b, s.x), mix(c, d, s.x), s.y);
}

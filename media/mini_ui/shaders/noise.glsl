/* A cheap hash of the four lattice points about a cell, its own, the next along, the next up and the next along and up, enough to scatter value noise;
 * a step along either axis adds the same to that axis's product, so the four share two multiplications. */
vec4 LatticeHashes(ivec2 cell)
{
	uvec2 x = uint(cell.x) * 0x8DA6B343u + uvec2(0u, 0x8DA6B343u);
	uvec2 y = uint(cell.y) * 0xD8163841u + uvec2(0u, 0xD8163841u);
	uvec4 h = uvec4(x.x ^ y.x, x.y ^ y.x, x.x ^ y.y, x.y ^ y.y);
	h = (h ^ (h >> 15u)) * 0x2C1B3C6Du;
	h ^= h >> 12u;
	return vec4(h) * (1.0 / 4294967296.0);
}

float Noise(vec2 p)
{
	vec2 f = fract(p);
	vec2 s = f * f * (3.0 - 2.0 * f);
	vec4 corners = LatticeHashes(ivec2(floor(p)));
	return mix(mix(corners.x, corners.y, s.x), mix(corners.z, corners.w, s.x), s.y);
}

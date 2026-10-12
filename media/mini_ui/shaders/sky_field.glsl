uniform usampler2D u_sky_field;

const float WALL_FACING = 2.0;
const float BLOCKED_SKY_RETURN = 0.3;
const uint SKY_SECTOR_MASK = 65535u;
const uint SKY_BYTE_MASK = 255u;
const float LEAST_SKY_WEIGHT = 1e-3;

float SkyOpenness(uvec4 horizons, float above, vec2 within, vec3 normal)
{
	mat2 turn = mat2(SKY_SECTOR_TURN, SKY_SECTOR_TURN, -SKY_SECTOR_TURN, SKY_SECTOR_TURN);
	vec2 way = vec2(1.0, 0.0);
	float open = 0.0;
	float weights = 0.0;
	for (int sector = 0; sector < SKY_SECTORS; sector++) {
		uint packed_sector = horizons.x & SKY_SECTOR_MASK;
		float height = float(packed_sector & SKY_BYTE_MASK) * SKY_HEIGHT_STEP;
		float run = float((packed_sector >> SKY_REACH_SHIFT) & SKY_BYTE_MASK) * SKY_REACH_STEP;
		float tangent = max(height - above, 0.0) / max(run - dot(within, way), NEAREST_HORIZON);
		float weight = max(normal.z + WALL_FACING * dot(way, normal.xy), 0.0);
		open += weight / (1.0 + tangent * tangent);
		weights += weight;
		if ((sector & 1) == 0) {
			horizons.x >>= SKY_SECTOR_BITS;
		} else {
			horizons = horizons.yzwx;
		}
		way = turn * way;
	}
	return weights > LEAST_SKY_WEIGHT ? mix(BLOCKED_SKY_RETURN, 1.0, open / weights) : 1.0;
}

float SkyOpenAt(ivec2 tile, vec3 at, vec3 normal)
{
	uvec4 horizons = texelFetch(u_sky_field, tile, 0);
	if (horizons == uvec4(0u)) return 1.0;
	float ground = LowestCorner(SurfaceOf(tile)) * LevelRise();
	return SkyOpenness(horizons, at.z - ground, at.xy - vec2(tile) - TILE_CENTRE, normal);
}

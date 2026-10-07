uniform sampler2D u_grain;
uniform sampler2D u_relief;

const float DETAIL_REPEAT_TILES = 4.0;
const float DETAIL_SLOPE_DECODING = 1.0 / 6.0;
const float DETAIL_TEXELS_PER_REPEAT = 512.0;
const float FACING_SHARPNESS = 4.0;
const mat2 DETAIL_TURN = mat2(0.8, 0.6, -0.6, 0.8);
const float UNSEEN_FACING = 0.02;
const float CLOSE_SCALE = 4.0;
const float CLOSE_FROM_PIXELS = 120.0;
const float CLOSE_TO_PIXELS = 260.0;
const float CLOSE_GRAIN = 0.7;
const float CLOSE_RELIEF = 0.6;

/* The ground's finest grain and relief at a point, read from the tiling detail onto whichever ways the surface faces, so steep ground keeps it as true as flat. */
struct Detail {
	float fine;
	float micro;
	float stones;
	float hollow;
	vec3 slope;
	vec3 stone_slope;
	vec3 facing;
	vec3 at;
};

struct DetailTap {
	vec4 grain;
	vec4 relief;
};

const DetailTap LEVEL_TAP = DetailTap(vec4(0.5), vec4(0.5));

DetailTap DetailTapAt(vec2 uv)
{
	return DetailTap(texture(u_grain, uv), texture(u_relief, uv));
}

/* A plane the surface hardly faces is not read; its level grain stands in, weighing next to nothing. */
DetailTap FacingTapAt(vec2 uv, float facing)
{
	return facing > UNSEEN_FACING ? DetailTapAt(uv) : LEVEL_TAP;
}

vec2 DecodedSlope(vec2 encoded)
{
	return DETAIL_TURN * ((encoded - 0.5) * 2.0 * DETAIL_SLOPE_DECODING * DETAIL_TEXELS_PER_REPEAT / DETAIL_REPEAT_TILES);
}

/* How squarely the surface faces up, east and north, sharpened so each plane takes over soon past the diagonal. */
vec3 FacingWeights(vec3 normal)
{
	vec3 facing = pow(abs(normal), vec3(FACING_SHARPNESS));
	return facing / (facing.x + facing.y + facing.z);
}

/* Noise of a frequency per tile laid onto the surface the way the detail is, so slopes show it as round as flat ground does; planes the surface hardly faces are skipped. */
float FacingOctave(Detail detail, float frequency, float offset)
{
	vec3 q = detail.at + offset;
	float sum = 0.0;
	float weight = 0.0;
	if (detail.facing.z > UNSEEN_FACING) { sum += Octave(q.xy, frequency) * detail.facing.z; weight += detail.facing.z; }
	if (detail.facing.x > UNSEEN_FACING) { sum += Octave(q.yz, frequency) * detail.facing.x; weight += detail.facing.x; }
	if (detail.facing.y > UNSEEN_FACING) { sum += Octave(q.xz, frequency) * detail.facing.y; weight += detail.facing.y; }
	return sum / weight;
}

/* A slope read on one of the three planes, laid back onto the two axes that plane spans. */
vec3 FacingSlope(DetailTap top, DetailTap east, DetailTap north, vec3 facing, bool stones)
{
	vec2 t = DecodedSlope(stones ? top.relief.zw : top.relief.xy);
	vec2 e = DecodedSlope(stones ? east.relief.zw : east.relief.xy);
	vec2 n = DecodedSlope(stones ? north.relief.zw : north.relief.xy);
	return vec3(t, 0.0) * facing.z + vec3(0.0, e) * facing.x + vec3(n.x, 0.0, n.y) * facing.y;
}

/* Where the eye comes so close that the detail's texels would show, a finer copy of its grain and bumps is read from above over the ground that faces up. */
Detail Closer(Detail detail, vec3 q)
{
	float close = smoothstep(CLOSE_FROM_PIXELS, CLOSE_TO_PIXELS, tile_pixels) * detail.facing.z;
	if (close <= 0.0) return detail;
	DetailTap finer = DetailTapAt(DETAIL_TURN * q.xy * CLOSE_SCALE + 0.53);
	detail.micro += (finer.grain.g - 0.5) * CLOSE_GRAIN * close;
	detail.slope.xy += DecodedSlope(finer.relief.xy) * CLOSE_RELIEF * close;
	return detail;
}

/* The detail is read from above and from both sides in render space, each turned off the map's lattice, and weighed by how squarely the surface faces each way. */
Detail DetailAt(vec3 render_point, vec3 normal)
{
	vec3 facing = FacingWeights(normal);
	vec3 q = render_point / DETAIL_REPEAT_TILES;
	DetailTap top = FacingTapAt(transpose(DETAIL_TURN) * q.xy, facing.z);
	DetailTap east = FacingTapAt(transpose(DETAIL_TURN) * q.yz + 0.37, facing.x);
	DetailTap north = FacingTapAt(transpose(DETAIL_TURN) * q.xz + 0.71, facing.y);
	vec4 grain = top.grain * facing.z + east.grain * facing.x + north.grain * facing.y;
	Detail detail = Detail(grain.r, grain.g, grain.b, grain.a - 0.5, FacingSlope(top, east, north, facing, false), FacingSlope(top, east, north, facing, true), facing, render_point);
	return Closer(detail, q);
}

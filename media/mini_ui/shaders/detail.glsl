uniform sampler2D u_grain;
uniform sampler2D u_relief;

const float DETAIL_REPEAT_TILES = 4.0;
const float DETAIL_SLOPE_DECODING = 1.0 / 6.0;
const float DETAIL_TEXELS_PER_REPEAT = 512.0;
const float FACING_SHARPNESS = 4.0;
const mat2 DETAIL_TURN = mat2(0.8, 0.6, -0.6, 0.8);

/* The ground's finest grain and relief at a point, read from the tiling detail onto whichever ways the surface faces, so steep ground keeps it as true as flat. */
struct Detail {
	float fine;
	float micro;
	float stones;
	float hollow;
	vec3 slope;
	vec3 stone_slope;
};

struct DetailTap {
	vec4 grain;
	vec4 relief;
};

DetailTap DetailTapAt(vec2 uv)
{
	return DetailTap(texture(u_grain, uv), texture(u_relief, uv));
}

vec2 DecodedSlope(vec2 encoded)
{
	return DETAIL_TURN * ((encoded - 0.5) * 2.0 * DETAIL_SLOPE_DECODING * DETAIL_TEXELS_PER_REPEAT / DETAIL_REPEAT_TILES);
}

/* A slope read on one of the three planes, laid back onto the two axes that plane spans. */
vec3 FacingSlope(DetailTap top, DetailTap east, DetailTap north, vec3 facing, bool stones)
{
	vec2 t = DecodedSlope(stones ? top.relief.zw : top.relief.xy);
	vec2 e = DecodedSlope(stones ? east.relief.zw : east.relief.xy);
	vec2 n = DecodedSlope(stones ? north.relief.zw : north.relief.xy);
	return vec3(t, 0.0) * facing.z + vec3(0.0, e) * facing.x + vec3(n.x, 0.0, n.y) * facing.y;
}

/* The detail is read from above and from both sides in render space, each turned off the map's lattice, and weighed by how squarely the surface faces each way. */
Detail DetailAt(vec3 render_point, vec3 normal)
{
	vec3 facing = pow(abs(normal), vec3(FACING_SHARPNESS));
	facing /= facing.x + facing.y + facing.z;
	vec3 q = render_point / DETAIL_REPEAT_TILES;
	DetailTap top = DetailTapAt(transpose(DETAIL_TURN) * q.xy);
	DetailTap east = DetailTapAt(transpose(DETAIL_TURN) * q.yz + 0.37);
	DetailTap north = DetailTapAt(transpose(DETAIL_TURN) * q.xz + 0.71);
	vec4 grain = top.grain * facing.z + east.grain * facing.x + north.grain * facing.y;
	return Detail(grain.r, grain.g, grain.b, grain.a - 0.5, FacingSlope(top, east, north, facing, false), FacingSlope(top, east, north, facing, true));
}

uniform usampler2D u_massing;

const vec3 UPWARD = vec3(0.0, 0.0, 1.0);
const uint MASSING_LOW_BYTE = 255u;
const uvec3 RGB565_SHIFTS = uvec3(11u, 5u, 0u);
const uvec3 RGB565_MASKS = uvec3(31u, 63u, 31u);
const uvec4 MASSING_INSET_SHIFTS = MASSING_INSET_SHIFT + uvec4(0u, 1u, 2u, 3u) * MASSING_INSET_BITS;

struct Massing {
	bool stands;
	vec2 low;
	vec2 high;
	float foot;
	float eaves;
	float top;
	vec3 wall;
	vec3 roof;
	uint grid;
	ivec2 anchor;
};

vec3 Rgb565(uint bits)
{
	return vec3((uvec3(bits) >> RGB565_SHIFTS) & RGB565_MASKS) / vec3(RGB565_MASKS);
}

ivec2 AnchorOf(ivec2 tile, uint traits)
{
	return tile - ivec2(notEqual(uvec2(traits) & uvec2(MASSING_FAR_X_BIT, MASSING_FAR_Y_BIT), uvec2(0u)));
}

ivec2 AnchorAt(ivec2 tile)
{
	return AnchorOf(tile, texelFetch(u_massing, Clamped(tile), 0).w);
}

float FormRise(uint texels)
{
	return float(texels) / TEXELS * FORM_HEIGHT_SCALE;
}

Massing MassingAt(ivec2 tile, Corners ground)
{
	uvec4 texel = texelFetch(u_massing, Clamped(tile), 0);
	uint traits = texel.w;
	vec4 insets = vec4((uvec4(traits) >> MASSING_INSET_SHIFTS) & MASSING_INSET_MOST) / MASSING_INSET_STEPS;
	float lowest = min(min(ground.north, ground.west), min(ground.east, ground.south));
	float foot = (lowest + float((traits >> MASSING_RAISED_SHIFT) & MASSING_RAISED_MOST)) * LevelRise();
	vec2 corner = vec2(tile);
	return Massing(texel.x != 0u, corner + insets.xz, corner + 1.0 - insets.yw, foot, foot + FormRise(texel.x >> MASSING_EAVES_SHIFT), foot + FormRise(texel.x & MASSING_LOW_BYTE),
		Rgb565(texel.y), Rgb565(texel.z), traits & MASSING_GRID_MASK, AnchorOf(tile, traits));
}

const float SNOW_LINE_PULL = 0.2;

struct Climate {
	float blanket;
	float lush;
};

float BlanketDepth(uvec4 codes)
{
	return Blanketed(codes.r) ? float((codes.g & DENSITY_MASK) + 1u) / float(DENSITY_MASK + 1u) : 0.0;
}

float Lushness(uvec4 codes)
{
	return (codes.g & LUSH_BIT) != 0u ? 1.0 : 0.0;
}

float Bilinear(vec4 corners, vec2 f)
{
	return mix(mix(corners.x, corners.y, f.x), mix(corners.z, corners.w, f.x), f.y);
}

Climate ClimateAt(vec2 p)
{
	vec2 q = p - TILE_CENTRE;
	ivec2 cell = ivec2(floor(q));
	vec2 f = smoothstep(0.0, 1.0, fract(q));
	uvec4 a = CodesAt(cell);
	uvec4 b = CodesAt(cell + ivec2(1, 0));
	uvec4 c = CodesAt(cell + ivec2(0, 1));
	uvec4 d = CodesAt(cell + ivec2(1, 1));
	float blanket = Bilinear(vec4(BlanketDepth(a), BlanketDepth(b), BlanketDepth(c), BlanketDepth(d)), f);
	float lush = Bilinear(vec4(Lushness(a), Lushness(b), Lushness(c), Lushness(d)), f);
	return Climate(blanket, lush);
}

float SnowLift(float level)
{
	return Landscape() == LANDSCAPE_ARCTIC ? clamp(level - SnowLine(), -1.0, 1.0) * SNOW_LINE_PULL : 0.0;
}

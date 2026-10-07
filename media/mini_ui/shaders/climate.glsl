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

/* The weights a quadratic B-spline gives the three tiles in a row about a point, t from the middle one's centre. */
vec3 QuadraticWeights(float t)
{
	return vec3(0.5 * (0.5 - t) * (0.5 - t), 0.75 - t * t, 0.5 * (0.5 + t) * (0.5 + t));
}

/* Each tile's blanket and lushness spread over the tiles about it by a quadratic B-spline, so their edges curve through the tiles instead of stepping along them. */
Climate ClimateAt(vec2 p)
{
	ivec2 middle = ivec2(floor(p));
	vec2 t = fract(p) - TILE_CENTRE;
	vec3 wx = QuadraticWeights(t.x);
	vec3 wy = QuadraticWeights(t.y);
	Climate climate = Climate(0.0, 0.0);
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			uvec4 codes = CodesAt(middle + ivec2(x, y));
			float weight = wx[x + 1] * wy[y + 1];
			climate.blanket += BlanketDepth(codes) * weight;
			climate.lush += Lushness(codes) * weight;
		}
	}
	return climate;
}

float SnowLift(float level)
{
	return Landscape() == LANDSCAPE_ARCTIC ? clamp(level - SnowLine(), -1.0, 1.0) * SNOW_LINE_PULL : 0.0;
}

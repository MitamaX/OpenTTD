uniform sampler2DArrayShadow u_shadow_map;

const float NORMAL_OFFSET_TEXELS = 1.2;
const float CASCADE_BLEND = 0.12;
const int FILTER_LOOKUPS = int(SHADOW_FILTER_SPAN) - 1;
const ivec4 LANES = ivec4(0, 1, 2, 3);

float ViewDepth(vec3 position)
{
	return -(u_view * vec4(position, 1.0)).z;
}

/* Filtered lookups weighing the texels a box SHADOW_FILTER_SPAN wide covers by 1 - f, 1, 1, 1 and f, for a point f into its texel. */
float[FILTER_LOOKUPS] FilterOffsets(float f)
{
	return float[FILTER_LOOKUPS](1.0 / (2.0 - f), 2.0, 3.0 + f / (1.0 + f));
}

float[FILTER_LOOKUPS] FilterWeights(float f)
{
	return float[FILTER_LOOKUPS](2.0 - f, 1.0, 1.0 + f);
}

/* A cascade's value of those the vector holds one per cascade, zero past either end, picked without indexing the vector by a variable, which some drivers misread. */
float OfCascade(vec4 per_cascade, int cascade)
{
	return dot(per_cascade, vec4(equal(ivec4(cascade), LANES)));
}

int CascadeHolding(float depth)
{
	return min(int(dot(vec4(greaterThan(vec4(depth), u_cascade_far)), vec4(1.0))), CASCADES - 1);
}

vec3 InCascade(int cascade, vec3 position)
{
	return (u_cascades[cascade] * vec4(position, 1.0)).xyz;
}

float FadedOut(float light, float depth)
{
	return mix(light, 1.0, smoothstep(u_shadow_fade.x, u_shadow_fade.y, depth));
}

/* The point is pushed off its surface by about a texel, more where the sun grazes it, so the surface never shadows itself;
 * a box of filtered lookups softens the edge over a few texels. */
float CascadeLight(int cascade, vec3 position, vec3 normal)
{
	float grazing = 1.0 - max(dot(normal, SunDirection()), 0.0);
	vec3 lifted = position + normal * OfCascade(u_cascade_texel, cascade) * NORMAL_OFFSET_TEXELS * (0.5 + grazing);
	vec3 at = InCascade(cascade, lifted);
	if (at.z >= 1.0) return 1.0;

	vec2 size = vec2(textureSize(u_shadow_map, 0).xy);
	vec2 texels = at.xy * size;
	vec2 first = floor(texels) - 0.5 * SHADOW_FILTER_SPAN + 0.5;
	vec2 f = texels - floor(texels);
	float x_offsets[FILTER_LOOKUPS] = FilterOffsets(f.x);
	float y_offsets[FILTER_LOOKUPS] = FilterOffsets(f.y);
	float x_weights[FILTER_LOOKUPS] = FilterWeights(f.x);
	float y_weights[FILTER_LOOKUPS] = FilterWeights(f.y);
	float lit = 0.0;
	for (int y = 0; y < FILTER_LOOKUPS; y++) {
		for (int x = 0; x < FILTER_LOOKUPS; x++) {
			vec2 tap = (first + vec2(x_offsets[x], y_offsets[y])) / size;
			lit += x_weights[x] * y_weights[y] * texture(u_shadow_map, vec4(tap, float(cascade), at.z));
		}
	}
	return lit / (SHADOW_FILTER_SPAN * SHADOW_FILTER_SPAN);
}

/* How much of the sun the world lets reach a point: the nearest cascade holding it decides, handing over to the next across a band, and shadows fade out where the cascades end. */
float CascadedLight(vec3 position, vec3 normal)
{
	float depth = ViewDepth(position);
	if (depth >= u_shadow_fade.y) return 1.0;

	int cascade = CascadeHolding(depth);
	float light = CascadeLight(cascade, position, normal);
	if (cascade < CASCADES - 1) {
		float end = OfCascade(u_cascade_far, cascade);
		float start = mix(OfCascade(u_cascade_far, cascade - 1), end, 1.0 - CASCADE_BLEND);
		if (depth > start) light = mix(light, CascadeLight(cascade + 1, position, normal), smoothstep(start, end, depth));
	}
	return FadedOut(light, depth);
}

float SunReaches(vec3 position)
{
	float depth = ViewDepth(position);
	int cascade = CascadeHolding(depth);
	vec3 at = InCascade(cascade, position);
	return FadedOut(at.z >= 1.0 ? 1.0 : texture(u_shadow_map, vec4(at.xy, float(cascade), at.z)), depth);
}

/* How much of the sun reaches a point past whatever stands in its way and the clouds drifting over, given the share the clouds let through. */
float SunVisibility(vec3 position, vec3 normal, float cloud_light)
{
	return CascadedLight(position, normal) * cloud_light;
}

float SunVisibility(vec3 position, vec3 normal)
{
	return SunVisibility(position, normal, CloudLight(position));
}

float FacingSunVisibility(vec3 position, vec3 normal)
{
	return dot(normal, SunDirection()) > 0.0 ? SunVisibility(position, normal) : 0.0;
}

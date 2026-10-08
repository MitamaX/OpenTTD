uniform sampler2DArrayShadow u_shadow_map;

const float NORMAL_OFFSET_TEXELS = 1.2;
const float CASCADE_BLEND = 0.12;
const float FILTER_SPAN = 4.0;
const int FILTER_LOOKUPS = 3;

float ViewDepth(vec3 position)
{
	return -(u_view * vec4(position, 1.0)).z;
}

/* A box of filtered lookups FILTER_SPAN texels wide weighs the five texels it covers along each axis by 1 - f, 1, 1, 1 and f, f being how far the point lies into its texel;
 * FILTER_LOOKUPS filtered lookups along the axis weigh them just so, the outer two each blending a pair: where each reads, counted from the first texel's centre, and how much it counts. */
vec3 FilterOffsets(float f)
{
	return vec3(1.0 / (2.0 - f), 2.0, 3.0 + f / (1.0 + f));
}

vec3 FilterWeights(float f)
{
	return vec3(2.0 - f, 1.0, 1.0 + f);
}

/* The point is pushed off its surface by about a texel, more where the sun grazes it, so the surface never shadows itself;
 * a box of filtered lookups softens the edge over a few texels. */
float CascadeLight(int cascade, vec3 position, vec3 normal)
{
	float grazing = 1.0 - max(dot(normal, SunDirection()), 0.0);
	vec3 lifted = position + normal * u_cascade_texel[cascade] * NORMAL_OFFSET_TEXELS * (0.5 + grazing);
	vec3 at = (u_cascades[cascade] * vec4(lifted, 1.0)).xyz;
	if (at.z >= 1.0) return 1.0;

	vec2 size = vec2(textureSize(u_shadow_map, 0).xy);
	vec2 texels = at.xy * size;
	vec2 first = floor(texels) - 0.5 * FILTER_SPAN + 0.5;
	vec2 f = texels - floor(texels);
	vec3 x_offsets = FilterOffsets(f.x);
	vec3 y_offsets = FilterOffsets(f.y);
	vec3 x_weights = FilterWeights(f.x);
	vec3 y_weights = FilterWeights(f.y);
	float lit = 0.0;
	for (int y = 0; y < FILTER_LOOKUPS; y++) {
		for (int x = 0; x < FILTER_LOOKUPS; x++) {
			vec2 tap = (first + vec2(x_offsets[x], y_offsets[y])) / size;
			lit += x_weights[x] * y_weights[y] * texture(u_shadow_map, vec4(tap, float(cascade), at.z));
		}
	}
	return lit / (FILTER_SPAN * FILTER_SPAN);
}

/* How much of the sun the world lets reach a point: the nearest cascade holding it decides, handing over to the next across a band, and shadows fade out where the cascades end. */
float CascadedLight(vec3 position, vec3 normal)
{
	float depth = ViewDepth(position);
	if (depth >= u_shadow_fade.y) return 1.0;

	int cascade = 0;
	while (cascade < CASCADES - 1 && depth > u_cascade_far[cascade]) cascade++;
	float light = CascadeLight(cascade, position, normal);
	if (cascade < CASCADES - 1) {
		float end = u_cascade_far[cascade];
		float start = mix(cascade == 0 ? 0.0 : u_cascade_far[cascade - 1], end, 1.0 - CASCADE_BLEND);
		if (depth > start) light = mix(light, CascadeLight(cascade + 1, position, normal), smoothstep(start, end, depth));
	}
	return mix(light, 1.0, smoothstep(u_shadow_fade.x, u_shadow_fade.y, depth));
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

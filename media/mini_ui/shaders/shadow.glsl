uniform sampler2DArrayShadow u_shadow_map;

const float NORMAL_OFFSET_TEXELS = 1.2;
const float CASCADE_BLEND = 0.12;
const int FILTER_SPAN = 4;

float ViewDepth(vec3 position)
{
	return -(u_view * vec4(position, 1.0)).z;
}

/* The point is pushed off its surface by about a texel, more where the sun grazes it, so the surface never shadows itself;
 * a grid of filtered lookups softens the edge over a few texels. */
float CascadeLight(int cascade, vec3 position, vec3 normal)
{
	float grazing = 1.0 - max(dot(normal, SunDirection()), 0.0);
	vec3 lifted = position + normal * u_cascade_texel[cascade] * NORMAL_OFFSET_TEXELS * (0.5 + grazing);
	vec3 at = (u_cascades[cascade] * vec4(lifted, 1.0)).xyz;
	if (at.z >= 1.0) return 1.0;

	vec2 texel = 1.0 / vec2(textureSize(u_shadow_map, 0).xy);
	float lit = 0.0;
	for (int y = 0; y < FILTER_SPAN; y++) {
		for (int x = 0; x < FILTER_SPAN; x++) {
			vec2 tap = (vec2(x, y) - 0.5 * float(FILTER_SPAN - 1)) * texel;
			lit += texture(u_shadow_map, vec4(at.xy + tap, float(cascade), at.z));
		}
	}
	return lit / float(FILTER_SPAN * FILTER_SPAN);
}

/* How much of the sun reaches a point: the nearest cascade holding it decides, handing over to the next across a band, and shadows fade out where the cascades end. */
float SunVisibility(vec3 position, vec3 normal)
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

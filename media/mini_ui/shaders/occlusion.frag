uniform sampler2D u_depth;

out vec2 frag_occlusion;

const float HALF_PI = 1.5707963;
const int SLICES = 3;
const int STEPS = 4;
const float RADIUS = 1.0;
const float FALLOFF_SHARE = 0.6;
const float LARGEST_PIXELS = 40.0;
const float SMALLEST_PIXELS = 1.0;
const float NORMAL_LENGTH_FLOOR = 0.05;
const float GOLDEN_STEP = 0.618034;

/* A 4 by 4 spread of offsets, so the blur that reads back four texels square takes every one of them in. */
float Interleaved(ivec2 texel)
{
	const float SPREAD[16] = float[16](0.0, 8.0, 2.0, 10.0, 12.0, 4.0, 14.0, 6.0, 3.0, 11.0, 1.0, 9.0, 15.0, 7.0, 13.0, 5.0);
	return (SPREAD[(texel.y & 3) * 4 + (texel.x & 3)] + 0.5) / 16.0;
}

/* How much of the sky above a point's surface the ground and solids around it leave open, horizon by horizon along a few slices through the view. */
float Openness(ivec2 full_texel, vec2 uv, vec2 half_size)
{
	vec3 centre = FetchViewPoint(u_depth, full_texel);
	vec3 normal = ViewNormal(u_depth, full_texel);
	vec3 view = normalize(-centre);
	float reach = RADIUS * u_projection[1][1] * 0.5 * half_size.y / -centre.z;
	if (reach < SMALLEST_PIXELS) return 1.0;
	reach = min(reach, LARGEST_PIXELS);

	ivec2 full_size = textureSize(u_depth, 0);
	float falloff_range = FALLOFF_SHARE * RADIUS;
	float falloff_scale = -1.0 / falloff_range;
	float falloff_offset = (RADIUS - falloff_range) / falloff_range + 1.0;
	float noise = Interleaved(ivec2(uv * half_size));
	float turn = fract(noise + GOLDEN_STEP * u_screen.w);

	float openness = 0.0;
	for (int slice = 0; slice < SLICES; slice++) {
		float angle = (float(slice) + turn) * (2.0 * HALF_PI / float(SLICES));
		vec2 omega = vec2(cos(angle), sin(angle));
		vec3 direction = vec3(omega, 0.0);
		vec3 ortho = direction - dot(direction, view) * view;
		vec3 axis = normalize(cross(ortho, view));
		vec3 projected = normal - axis * dot(normal, axis);
		float projected_length = length(projected);
		float cos_normal = clamp(dot(projected, view) / max(projected_length, 1e-4), 0.0, 1.0);
		float n = sign(dot(ortho, projected)) * acos(cos_normal);
		float low0 = cos(n + HALF_PI);
		float low1 = cos(n - HALF_PI);
		float horizon0 = low0;
		float horizon1 = low1;

		for (int step = 0; step < STEPS; step++) {
			float share = (float(step) + fract(noise * 7.0 + 0.5 * float(slice))) / float(STEPS);
			vec2 offset = omega * max(share * share * reach, float(step) + 1.0) / half_size;
			ivec2 tap0 = clamp(ivec2((uv + offset) * vec2(full_size)), ivec2(0), full_size - 1);
			ivec2 tap1 = clamp(ivec2((uv - offset) * vec2(full_size)), ivec2(0), full_size - 1);
			vec3 delta0 = FetchViewPoint(u_depth, tap0) - centre;
			vec3 delta1 = FetchViewPoint(u_depth, tap1) - centre;
			float distance0 = length(delta0);
			float distance1 = length(delta1);
			float weight0 = clamp(distance0 * falloff_scale + falloff_offset, 0.0, 1.0);
			float weight1 = clamp(distance1 * falloff_scale + falloff_offset, 0.0, 1.0);
			horizon0 = max(horizon0, mix(low0, dot(delta0 / max(distance0, 1e-4), view), weight0));
			horizon1 = max(horizon1, mix(low1, dot(delta1 / max(distance1, 1e-4), view), weight1));
		}

		float h0 = n + clamp(-acos(horizon1) - n, -HALF_PI, HALF_PI);
		float h1 = n + clamp(acos(horizon0) - n, -HALF_PI, HALF_PI);
		float arc0 = (cos_normal + 2.0 * h0 * sin(n) - cos(2.0 * h0 - n)) * 0.25;
		float arc1 = (cos_normal + 2.0 * h1 * sin(n) - cos(2.0 * h1 - n)) * 0.25;
		openness += mix(projected_length, 1.0, NORMAL_LENGTH_FLOOR) * (arc0 + arc1);
	}
	return clamp(openness / float(SLICES), 0.0, 1.0);
}

/* Each texel answers for the full size texel at its own corner, and keeps how far ahead that lies so the blur can tell surfaces apart. */
void main()
{
	vec2 half_size = vec2(textureSize(u_depth, 0) / 2);
	ivec2 full_texel = ivec2(gl_FragCoord.xy) * 2;
	float depth = texelFetch(u_depth, full_texel, 0).r;
	if (depth >= SKY_DEPTH) {
		frag_occlusion = vec2(1.0, u_lens.y);
		return;
	}
	vec2 uv = (vec2(full_texel) + 0.5) / vec2(textureSize(u_depth, 0));
	frag_occlusion = vec2(Openness(full_texel, uv, half_size), AheadOf(depth));
}

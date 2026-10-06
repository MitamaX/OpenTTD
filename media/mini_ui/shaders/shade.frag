uniform sampler2D u_colour;
uniform sampler2D u_depth;
uniform sampler2D u_occlusion;
uniform float u_occlusion_strength;

out vec4 frag_colour;

const int BLUR_SPAN = 4;
const float BLUR_DEPTH_TOLERANCE = 0.04;
const float OCCLUSION_POWER = 1.5;

/* Past where the camera's fog sets in the ground fades on into the haze, so the far clip never shows. */
float FarFade(float distance)
{
	float t = clamp((distance - u_lens.z) / max(u_lens.w - u_lens.z, 1e-3), 0.0, 1.0);
	return t * t * (3.0 - 2.0 * t);
}

/* The half size occlusion blurred over four texels square, which takes in its whole noise spread, leaving out texels on other surfaces. */
float Occlusion(ivec2 texel, float ahead)
{
	ivec2 origin = texel / 2 - 1;
	ivec2 last = textureSize(u_occlusion, 0) - 1;
	float tolerance = ahead * BLUR_DEPTH_TOLERANCE;
	float sum = 0.0;
	float weights = 0.0;
	for (int y = 0; y < BLUR_SPAN; y++) {
		for (int x = 0; x < BLUR_SPAN; x++) {
			vec2 tap = texelFetch(u_occlusion, clamp(origin + ivec2(x, y), ivec2(0), last), 0).rg;
			float weight = max(0.0, 1.0 - abs(tap.y - ahead) / tolerance);
			sum += tap.x * weight;
			weights += weight;
		}
	}
	return weights > 1e-3 ? sum / weights : 1.0;
}

/* Occlusion only dims the light of the sky and the ground around, so it bites less where the sun falls full on a surface. */
vec3 Occluded(vec3 colour, ivec2 texel, float ahead, vec3 point)
{
	if (u_occlusion_strength <= 0.0) return colour;
	vec3 normal = WorldDirection(ViewNormal(u_depth, texel));
	float ambient = Luminance(AmbientLight(normal));
	float sun = Luminance(SunRadiance()) * max(dot(normal, SunDirection()), 0.0) * SunVisibility(point, normal);
	float open = mix(1.0, pow(Occlusion(texel, ahead), OCCLUSION_POWER), u_occlusion_strength);
	return colour * (1.0 - ambient / (ambient + sun) * (1.0 - open));
}

void main()
{
	ivec2 texel = ivec2(gl_FragCoord.xy);
	vec3 sight = SightAt(gl_FragCoord.xy);
	float depth = texelFetch(u_depth, texel, 0).r;
	if (depth >= SKY_DEPTH) {
		frag_colour = vec4(SkyRadiance(sight) + SunDisc(sight), 1.0);
		return;
	}

	float distance = SightDistance(depth, sight);
	vec3 point = Eye() + sight * distance;
	vec3 surface = Occluded(texelFetch(u_colour, texel, 0).rgb, texel, AheadOf(depth), point);
	frag_colour = vec4(mix(Hazed(surface, Eye(), point), Haze(sight), FarFade(distance)), 1.0);
}

uniform sampler2D u_colour;
uniform sampler2D u_depth;
uniform sampler2D u_occlusion;
uniform float u_occlusion_strength;

out vec4 frag_colour;

const int BLUR_SPAN = 4;
const float BLUR_DEPTH_TOLERANCE = 0.04;
const float OCCLUSION_POWER = 1.5;
const float CONTACT_ON_SUN = 0.6;
const float CLEAR_SHARE_OF_FOCUS = 0.8;

/* Past where the camera's fog sets in the ground fades on into the haze, so the far clip never shows. */
float FarFade(float distance)
{
	float t = clamp((distance - u_lens.z) / max(u_lens.y - u_lens.z, 1e-3), 0.0, 1.0);
	return t * t * (3.0 - 2.0 * t);
}

/* The half size openness, over a tile around and where things meet, blurred over four texels square, which takes in its whole noise spread, leaving out texels on other surfaces. */
vec2 Openness(ivec2 texel, float ahead)
{
	ivec2 origin = texel / 2 - 1;
	ivec2 last = textureSize(u_occlusion, 0) - 1;
	float tolerance = ahead * BLUR_DEPTH_TOLERANCE;
	vec2 sum = vec2(0.0);
	float weights = 0.0;
	for (int y = 0; y < BLUR_SPAN; y++) {
		for (int x = 0; x < BLUR_SPAN; x++) {
			vec3 tap = texelFetch(u_occlusion, clamp(origin + ivec2(x, y), ivec2(0), last), 0).rgb;
			float weight = max(0.0, 1.0 - abs(tap.z - ahead) / tolerance);
			sum += tap.xy * weight;
			weights += weight;
		}
	}
	return weights > 1e-3 ? sum / weights : vec2(1.0);
}

/* The light of the sky and the ground around, the share of a surface's light its shader left in the alpha, dims by how closed in it lies over a tile around and where things meet;
 * where things meet they shade a little of each other's sunlight too, which the sun's shadows are too coarse to catch. */
vec3 Occluded(vec4 surface, ivec2 texel, float ahead)
{
	if (u_occlusion_strength <= 0.0) return surface.rgb;
	vec2 open = mix(vec2(1.0), pow(Openness(texel, ahead), vec2(OCCLUSION_POWER)), u_occlusion_strength);
	float ambient = open.x * open.y;
	float sun = mix(1.0, open.y, CONTACT_ON_SUN);
	return surface.rgb * mix(sun, ambient, surface.a);
}

/* The air short of what the view looks at is left clear, so only what lies beyond it recedes into the haze. */
void main()
{
	ivec2 texel = ivec2(gl_FragCoord.xy);
	vec3 sight = SightAt(gl_FragCoord.xy);
	float depth = texelFetch(u_depth, texel, 0).r;
	if (depth >= SKY_DEPTH) {
		frag_colour = vec4(CloudedSky(sight) + SunDisc(sight), 1.0);
		return;
	}

	float distance = SightDistance(depth, sight);
	vec3 point = Eye() + sight * distance;
	vec3 surface = Occluded(texelFetch(u_colour, texel, 0), texel, AheadOf(depth));
	vec3 clear = Eye() + sight * min(distance, FocusDistance() * CLEAR_SHARE_OF_FOCUS);
	frag_colour = vec4(mix(Hazed(surface, sight, clear, point), Haze(sight), FarFade(distance)), 1.0);
}

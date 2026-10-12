uniform sampler2D u_colour;
uniform sampler2D u_depth;
uniform sampler2D u_occlusion;
uniform float u_occlusion_strength;

out vec4 frag_colour;

const int BLUR_SPAN = 4;
const float BLUR_TENT_REACH = float(BLUR_SPAN) * 0.5;
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

float[BLUR_SPAN] TentWeights(int first, float centre)
{
	float[BLUR_SPAN] weights;
	for (int tap = 0; tap < BLUR_SPAN; tap++) weights[tap] = max(1.0 - abs(float(first + tap) - centre) / BLUR_TENT_REACH, 0.0);
	return weights;
}

/* The half size openness, over a tile around and where things meet, blurred over four texels square, which takes in its whole noise spread, leaving out texels on other surfaces. */
vec3 Openness(ivec2 texel, float ahead)
{
	vec2 at = vec2(texel) * 0.5;
	ivec2 origin = ivec2(floor(at)) - (BLUR_SPAN / 2 - 1);
	float x_tent[BLUR_SPAN] = TentWeights(origin.x, at.x);
	float y_tent[BLUR_SPAN] = TentWeights(origin.y, at.y);
	ivec2 last = textureSize(u_occlusion, 0) - 1;
	float tolerance = ahead * BLUR_DEPTH_TOLERANCE;
	vec3 sum = vec3(0.0);
	float weights = 0.0;
	for (int y = 0; y < BLUR_SPAN; y++) {
		for (int x = 0; x < BLUR_SPAN; x++) {
			vec4 tap = texelFetch(u_occlusion, clamp(origin + ivec2(x, y), ivec2(0), last), 0);
			float weight = max(0.0, 1.0 - abs(tap.z - ahead) / tolerance) * x_tent[x] * y_tent[y];
			sum += tap.xyw * weight;
			weights += weight;
		}
	}
	return weights > 1e-3 ? sum / weights : vec3(1.0);
}

/* The ambient share a surface's shader left in the alpha dims by how closed in it lies, and where things meet a little of the sunlight too, which the sun's shadows are too coarse to catch. */
vec3 Occluded(vec4 surface, ivec2 texel, float ahead)
{
	if (u_occlusion_strength <= 0.0) return surface.rgb;
	vec3 blurred = Openness(texel, ahead);
	vec2 open = mix(vec2(1.0), pow(blurred.xy, vec2(OCCLUSION_POWER)), u_occlusion_strength);
	float sky = mix(1.0, blurred.z, u_occlusion_strength);
	float ambient = open.x * sky * open.y;
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

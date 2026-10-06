uniform sampler2D u_colour;
uniform sampler2D u_depth;

out vec4 frag_colour;

const float EXPOSURE = 0.33;
const float SATURATION = 0.88;
const float DITHER = 1.0 / 255.0;
const float DISPLAY_GAMMA = 1.0 / 2.2;
const vec3 LUMINANCE = vec3(0.2126, 0.7152, 0.0722);

/* Past where the camera's fog sets in the ground fades on into the haze, so the far clip never shows. */
float FarFade(float distance)
{
	float t = clamp((distance - u_lens.z) / max(u_lens.w - u_lens.z, 1e-3), 0.0, 1.0);
	return t * t * (3.0 - 2.0 * t);
}

/* A touch less saturated, then a filmic curve: a gentle toe, and a shoulder that rolls highlights off toward white instead of clipping. */
vec3 Tonemap(vec3 radiance)
{
	vec3 exposed = radiance * EXPOSURE;
	vec3 x = max(mix(vec3(dot(exposed, LUMINANCE)), exposed, SATURATION), 0.0);
	return clamp(x * (2.51 * x + 0.03) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

/* Half a display step of noise breaks the sky's smooth gradients into steps too fine to see. */
float Dither(vec2 fragment)
{
	return (ScreenNoise(fragment) - 0.5) * DITHER;
}

void main()
{
	ivec2 texel = ivec2(gl_FragCoord.xy);
	vec3 sight = SightAt(gl_FragCoord.xy);
	float depth = texelFetch(u_depth, texel, 0).r;
	vec3 radiance = SkyRadiance(sight) + SunDisc(sight);
	if (depth < 1.0) {
		float distance = SightDistance(depth, sight);
		vec3 point = Eye() + sight * distance;
		radiance = mix(Hazed(texelFetch(u_colour, texel, 0).rgb, Eye(), point), Haze(sight), FarFade(distance));
	}
	frag_colour = vec4(pow(Tonemap(radiance), vec3(DISPLAY_GAMMA)) + Dither(gl_FragCoord.xy), 1.0);
}

uniform sampler2D u_resolved;
uniform sampler2D u_bloom;
uniform sampler2D u_depth;
uniform float u_exposure;
uniform float u_bloom_strength;
uniform float u_sharpen;
uniform float u_focus;
uniform float u_blur_pixels;

out vec4 frag_colour;

const float DITHER = 1.0 / 255.0;
const float DISPLAY_GAMMA = 1.0 / 2.2;

const float AGX_LOWEST_EV = -12.47393;
const float AGX_HIGHEST_EV = 4.026069;
const mat3 AGX_INSET = mat3(
	0.842479062253094, 0.0423282422610123, 0.0423756549057051,
	0.0784335999999992, 0.878468636469772, 0.0784336,
	0.0792237451477643, 0.0791661274605434, 0.879142973793104);
const mat3 AGX_OUTSET = mat3(
	1.19687900512017, -0.0528968517574562, -0.0529716355144438,
	-0.0980208811401368, 1.15190312990417, -0.0980434501171241,
	-0.0990297440797205, -0.0989611768448433, 1.15107367264116);
const float LOOK_POWER = 1.2;
const float LOOK_SATURATION = 1.15;

const vec3 SHADOW_TINT = vec3(0.965, 0.995, 1.05);
const vec3 HIGHLIGHT_TINT = vec3(1.035, 1.0, 0.955);
const float TINT_SPLIT = 0.45;
const float VIGNETTE_DEPTH = 0.2;

const int BLUR_TAPS = 24;
const float GOLDEN_ANGLE = 2.39996323;
const float LARGEST_BLUR_PIXELS = 12.0;

/* AgX's sigmoid in log space: a gentle toe, and a shoulder that rolls highlights toward white while their hue drifts as film's would. */
vec3 AgxCurve(vec3 x)
{
	vec3 x2 = x * x;
	vec3 x4 = x2 * x2;
	return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}

/* Scene light to display light. */
vec3 Tonemap(vec3 radiance)
{
	vec3 inset = AGX_INSET * max(radiance, 1e-10);
	vec3 encoded = AgxCurve((clamp(log2(inset), AGX_LOWEST_EV, AGX_HIGHEST_EV) - AGX_LOWEST_EV) / (AGX_HIGHEST_EV - AGX_LOWEST_EV));
	encoded = pow(max(encoded, 0.0), vec3(LOOK_POWER));
	float luma = Luminance(encoded);
	encoded = luma + LOOK_SATURATION * (encoded - luma);
	return pow(max(AGX_OUTSET * encoded, 0.0), vec3(1.0 / DISPLAY_GAMMA));
}

/* Shadows lean a touch cool and highlights a touch warm, as a sunny afternoon reads. */
vec3 Graded(vec3 display_light)
{
	float luma = Luminance(display_light);
	return display_light * mix(SHADOW_TINT, HIGHLIGHT_TINT, smoothstep(0.0, TINT_SPLIT, luma));
}

float Vignette(vec2 uv)
{
	vec2 off = (uv - 0.5) * vec2(u_screen.x / u_screen.y, 1.0);
	float reach = dot(off, off) / dot(vec2(0.5 * u_screen.x / u_screen.y, 0.5), vec2(0.5 * u_screen.x / u_screen.y, 0.5));
	return 1.0 - VIGNETTE_DEPTH * reach * sqrt(reach);
}

/* How many pixels wide a point's blur spreads, by how far it lies from the focus. */
float BlurPixels(float depth)
{
	float ahead = depth >= SKY_DEPTH ? u_lens.y : AheadOf(depth);
	return min(u_blur_pixels * abs(1.0 - u_focus / ahead), LARGEST_BLUR_PIXELS);
}

/* Taps spread on a golden spiral over the pixel's blur; each counts only where its own blur reaches back over the pixel, so sharp things in front stay sharp. */
vec3 Defocused(vec2 uv, vec3 sharp)
{
	vec2 texel = 1.0 / vec2(textureSize(u_resolved, 0));
	float own = BlurPixels(texelFetch(u_depth, ivec2(gl_FragCoord.xy), 0).r);
	if (own < 0.5) return sharp;
	vec3 sum = sharp;
	float weights = 1.0;
	for (int i = 1; i < BLUR_TAPS; i++) {
		float reach = sqrt(float(i) / float(BLUR_TAPS)) * own;
		vec2 tap = uv + vec2(cos(float(i) * GOLDEN_ANGLE), sin(float(i) * GOLDEN_ANGLE)) * reach * texel;
		float weight = clamp(BlurPixels(textureLod(u_depth, tap, 0.0).r) - reach + 1.0, 0.0, 1.0);
		sum += textureLod(u_resolved, tap, 0.0).rgb * weight;
		weights += weight;
	}
	return sum / weights;
}

/* The temporal blend softens a little, which a light unsharp mask over the four neighbours gives back. */
vec3 Sharpened(ivec2 texel, vec3 centre)
{
	if (u_sharpen <= 0.0) return centre;
	ivec2 last = textureSize(u_resolved, 0) - 1;
	vec3 around = texelFetch(u_resolved, clamp(texel + ivec2(1, 0), ivec2(0), last), 0).rgb;
	around += texelFetch(u_resolved, clamp(texel - ivec2(1, 0), ivec2(0), last), 0).rgb;
	around += texelFetch(u_resolved, clamp(texel + ivec2(0, 1), ivec2(0), last), 0).rgb;
	around += texelFetch(u_resolved, clamp(texel - ivec2(0, 1), ivec2(0), last), 0).rgb;
	return max(centre + (centre - around * 0.25) * u_sharpen, 0.0);
}

/* Half a display step of noise breaks the sky's smooth gradients into steps too fine to see. */
float Dither(vec2 fragment)
{
	return (ScreenNoise(fragment) - 0.5) * DITHER;
}

void main()
{
	ivec2 texel = ivec2(gl_FragCoord.xy);
	vec2 uv = gl_FragCoord.xy / u_screen.xy;
	vec3 radiance = Sharpened(texel, texelFetch(u_resolved, texel, 0).rgb);
	if (u_focus > 0.0) radiance = Defocused(uv, radiance);
	radiance = mix(radiance, textureLod(u_bloom, uv, 0.0).rgb, u_bloom_strength);
	vec3 display_light = Graded(Tonemap(radiance * u_exposure)) * Vignette(uv);
	frag_colour = vec4(pow(display_light, vec3(DISPLAY_GAMMA)) + Dither(gl_FragCoord.xy), 1.0);
}

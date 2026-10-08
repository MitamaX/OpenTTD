const vec3 SKY_LIGHT = vec3(0.34, 0.46, 0.68);
const vec3 GROUND_ALBEDO = vec3(0.155, 0.135, 0.098);
const float SKY_TOWARD_SUN = 0.15;
const float DIELECTRIC_REFLECTANCE = 0.04;
const float GLASS_REFLECTANCE = 0.2;
const float SMOOTHEST = 0.04;
const vec4 RESPONSE_FIT_SLOPE = vec4(-1.0, -0.0275, -0.572, 0.022);
const vec4 RESPONSE_FIT_BASE = vec4(1.0, 0.0425, 1.04, -0.04);
const float RESPONSE_FIT_GRAZING = 1.04;
const float RESPONSE_FIT_FALLOFF = 9.28;
const float GAMMA = 2.2;
const float GLOW_FOCUS_POWER = 4.0;

/* Colours picked on screen to the light they reflect. */
vec3 Linear(vec3 display)
{
	return pow(max(display, 0.0), vec3(GAMMA));
}

float Fresnel(float cosine, float reflectance)
{
	return reflectance + (1.0 - reflectance) * pow(1.0 - clamp(cosine, 0.0, 1.0), 5.0);
}

/* The light the ground around throws back of the sun and sky falling on it. */
vec3 GroundBounce()
{
	return GROUND_ALBEDO * (SunRadiance() * SunDirection().z + SKY_LIGHT);
}

/* The light of the sky dome and of the ground around a surface facing this way; the sky is a little brighter toward the sun. */
vec3 AmbientLight(vec3 normal)
{
	vec2 toward = normalize(SunDirection().xy);
	vec3 sky = SKY_LIGHT * (1.0 + SKY_TOWARD_SUN * dot(normal.xy, toward));
	return mix(GroundBounce(), sky, normal.z * 0.5 + 0.5);
}

vec3 Diffuse(vec3 albedo, vec3 sun, vec3 normal, float occlusion, float reflectance)
{
	return albedo * (1.0 - reflectance) * (sun + AmbientLight(normal) * occlusion);
}

/* A microfacet highlight, scaled so that a surface lit straight on by light it reflects whole comes out as bright as that light. */
float Highlight(vec3 normal, vec3 view, vec3 light, float roughness, float reflectance)
{
	vec3 half_way = normalize(view + light);
	float alpha = max(roughness * roughness, SMOOTHEST * SMOOTHEST);
	float alpha_squared = alpha * alpha;
	float aligned = max(dot(normal, half_way), 0.0);
	float spread = aligned * aligned * (alpha_squared - 1.0) + 1.0;
	float facets = alpha_squared / (spread * spread);
	float k = alpha * 0.5;
	float seen = max(dot(normal, view), 1e-3);
	float lit = max(dot(normal, light), 1e-3);
	float masking = 1.0 / ((seen * (1.0 - k) + k) * (lit * (1.0 - k) + k));
	return facets * masking * 0.25 * Fresnel(dot(half_way, view), reflectance);
}

/* How much of its surroundings a surface this rough reflects seen at this angle, from Karis's fit to the microfacet response summed over every way the light comes in. */
float SurroundingReflectance(float roughness, float facing, float reflectance)
{
	vec4 fit = roughness * RESPONSE_FIT_SLOPE + RESPONSE_FIT_BASE;
	float grazing = (min(fit.x * fit.x, exp2(-RESPONSE_FIT_FALLOFF * facing)) * fit.x + fit.y) * RESPONSE_FIT_GRAZING;
	return reflectance * (fit.z - grazing) + fit.w + grazing;
}

/* What a surface mirrors along a way: the sky above the horizon, and below it the ground, lost in the haze where it lies far off toward the horizon. */
vec3 Surroundings(vec3 way)
{
	return way.z > 0.0 ? SkyRadiance(way) : mix(Haze(way), GroundBounce(), smoothstep(0.0, 1.0, -way.z));
}

/* The sky and ground a surface reflects, blurring toward the light falling on it from all around the rougher it is. */
vec3 Sheen(vec3 normal, vec3 view, float roughness, float reflectance)
{
	vec3 around = mix(Surroundings(reflect(-view, normal)), AmbientLight(normal), roughness * roughness);
	return around * SurroundingReflectance(roughness, max(dot(normal, view), 0.0), reflectance);
}

/* The share of the light falling on a surface that comes from the sky and the ground around, which is all the occlusion found over the screen dims;
 * the world's shaders leave it in the alpha of what they draw. */
float AmbientShare(vec3 normal, float sunlit)
{
	float ambient = Luminance(AmbientLight(normal));
	float sun = Luminance(SunRadiance()) * max(dot(normal, SunDirection()), 0.0) * sunlit;
	return ambient / (ambient + sun);
}

/* The light a surface point's coat reflects toward the eye: the sun's highlight, by the share of the sun that reaches it, and the sheen of its surroundings dimmed by occlusion;
 * reflectance is how much of the light it reflects head on. */
vec3 Reflected(vec3 normal, vec3 position, float roughness, float reflectance, float occlusion, float sunlit)
{
	vec3 light = SunDirection();
	vec3 view = normalize(Eye() - position);
	vec3 sun = SunRadiance() * max(dot(normal, light), 0.0) * sunlit;
	return sun * Highlight(normal, view, light, roughness, reflectance) + Sheen(normal, view, roughness, reflectance) * occlusion;
}

/* The light a surface point sends toward the eye with the share of the sun that reaches it given: sunlight, sky and ground light dimmed by occlusion, what its coat reflects and the sun's glints off crystals. */
vec4 SunlitRadiance(vec3 albedo, vec3 normal, vec3 position, float roughness, float reflectance, float occlusion, float glint, float sunlit)
{
	vec3 sun = SunRadiance() * max(dot(normal, SunDirection()), 0.0) * sunlit;
	vec3 reflected = Reflected(normal, position, roughness, reflectance, occlusion, sunlit) + sun * glint;
	return vec4(Diffuse(albedo, sun, normal, occlusion, reflectance) + reflected, AmbientShare(normal, sunlit));
}

/* The light a surface point sends toward the eye, the sun's shut out where it is shadowed, with its ambient share.
 * Every lit world shader shades through this, with albedo in linear light and the point and normal in render space. */
vec4 Radiance(vec3 albedo, vec3 normal, vec3 position, float roughness, float reflectance, float occlusion)
{
	return SunlitRadiance(albedo, normal, position, roughness, reflectance, occlusion, 0.0, SunVisibility(position, normal));
}

/* Leaves let light through: the sun wraps on past where they turn away from it, and glows through them where they are seen against it. */
vec4 FoliageRadiance(vec3 albedo, vec3 normal, vec3 position, float translucency, float occlusion, float sunlit)
{
	vec3 light = SunDirection();
	vec3 view = normalize(Eye() - position);
	float wrapped = max(dot(normal, light) + translucency, 0.0) / (1.0 + translucency);
	float glow = translucency * pow(max(dot(-view, light), 0.0), GLOW_FOCUS_POWER);
	vec3 sun = SunRadiance() * (wrapped + glow) * sunlit;
	return vec4(Diffuse(albedo, sun, normal, occlusion, DIELECTRIC_REFLECTANCE), AmbientShare(normal, sunlit));
}

const vec3 SKY_LIGHT = vec3(0.58, 0.76, 1.10);
const vec3 GROUND_BOUNCE = vec3(0.30, 0.26, 0.20);
const float SKY_TOWARD_SUN = 0.15;
const float DIELECTRIC_REFLECTANCE = 0.04;
const float SMOOTHEST = 0.04;
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

/* The light of the sky dome and of the ground around a surface facing this way; the sky is a little brighter toward the sun. */
vec3 AmbientLight(vec3 normal)
{
	vec2 toward = normalize(SunDirection().xy);
	vec3 sky = SKY_LIGHT * (1.0 + SKY_TOWARD_SUN * dot(normal.xy, toward));
	return mix(GROUND_BOUNCE, sky, normal.z * 0.5 + 0.5);
}

vec3 Diffuse(vec3 albedo, vec3 sun, vec3 normal, float occlusion)
{
	return albedo * (1.0 - DIELECTRIC_REFLECTANCE) * (sun + AmbientLight(normal) * occlusion);
}

/* A microfacet highlight, scaled so that a surface lit straight on by light it reflects whole comes out as bright as that light. */
float Highlight(vec3 normal, vec3 view, vec3 light, float roughness)
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
	return facets * masking * 0.25 * Fresnel(dot(half_way, view), DIELECTRIC_REFLECTANCE);
}

/* The light a surface point sends toward the eye: sunlight unless shadowed, sky and ground light dimmed by occlusion, and the sun's highlight.
 * Every lit world shader shades through this, with albedo in linear light and the point and normal in render space. */
vec3 Radiance(vec3 albedo, vec3 normal, vec3 position, float roughness, float occlusion)
{
	vec3 light = SunDirection();
	vec3 view = normalize(Eye() - position);
	vec3 sun = SunRadiance() * max(dot(normal, light), 0.0) * SunVisibility(position, normal);
	return Diffuse(albedo, sun, normal, occlusion) + sun * Highlight(normal, view, light, roughness);
}

/* Leaves let light through: the sun wraps on past where they turn away from it, and glows through them where they are seen against it. */
vec3 FoliageRadiance(vec3 albedo, vec3 normal, vec3 position, float translucency, float occlusion)
{
	vec3 light = SunDirection();
	vec3 view = normalize(Eye() - position);
	float wrapped = max(dot(normal, light) + translucency, 0.0) / (1.0 + translucency);
	float glow = translucency * pow(max(dot(-view, light), 0.0), GLOW_FOCUS_POWER);
	vec3 sun = SunRadiance() * (wrapped + glow) * SunVisibility(position, normal);
	return Diffuse(albedo, sun, normal, occlusion);
}

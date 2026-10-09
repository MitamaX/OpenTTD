const vec3 SKY_LIGHT = vec3(0.49, 0.58, 0.69);
const vec3 GROUND_ALBEDO = vec3(0.155, 0.135, 0.098);
const float SKY_TOWARD_SUN = 0.15;
const float DIELECTRIC_REFLECTANCE = 0.04;
const float GLASS_REFLECTANCE = 0.2;
const float PANE_REFLECTANCE = 0.08;
const float PANE_SKY_LIFT = 0.04;
const float ROOM_SUNLIT_SHARE = 0.12;
const vec3 LAMP_LIGHT = vec3(1.6, 1.15, 0.67);
const float SKYLINE_FLATTENING = 0.5;
const float SKYLINE_SHARE = 0.6;
const float SKYLINE_SATURATION = 0.45;
const vec3 TOWN_ALBEDO = vec3(0.26, 0.25, 0.23);
const float TOWN_REACH = 5.0;
const float SMOOTHEST = 0.04;
const vec4 RESPONSE_FIT_SLOPE = vec4(-1.0, -0.0275, -0.572, 0.022);
const vec4 RESPONSE_FIT_BASE = vec4(1.0, 0.0425, 1.04, -0.04);
const float RESPONSE_FIT_GRAZING = 1.04;
const float RESPONSE_FIT_FALLOFF = 9.28;
const float GLOW_FOCUS_POWER = 4.0;

struct Coat {
	float roughness;
	float reflectance;
};

struct Pane {
	vec3 room;
	vec3 film;
	float reflectance;
	float lamp;
};

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

vec3 Sunlight(vec3 normal, float sunlit)
{
	return SunRadiance() * max(dot(normal, SunDirection()), 0.0) * sunlit;
}

vec3 Diffuse(vec3 albedo, vec3 light, float reflectance)
{
	return albedo * (1.0 - reflectance) * light;
}

/* A microfacet highlight, scaled so that a surface lit straight on by light it reflects whole comes out as bright as that light. */
float Highlight(vec3 normal, vec3 view, vec3 light, Coat coat)
{
	vec3 half_way = normalize(view + light);
	float alpha = max(coat.roughness * coat.roughness, SMOOTHEST * SMOOTHEST);
	float alpha_squared = alpha * alpha;
	float aligned = max(dot(normal, half_way), 0.0);
	float spread = aligned * aligned * (alpha_squared - 1.0) + 1.0;
	float facets = alpha_squared / (spread * spread);
	float k = alpha * 0.5;
	float seen = max(dot(normal, view), 1e-3);
	float lit = max(dot(normal, light), 1e-3);
	float masking = 1.0 / ((seen * (1.0 - k) + k) * (lit * (1.0 - k) + k));
	return facets * masking * 0.25 * Fresnel(dot(half_way, view), coat.reflectance);
}

/* How much of its surroundings a coat reflects seen at this angle, from Karis's fit to the microfacet response summed over every way the light comes in. */
float SurroundingReflectance(Coat coat, float facing)
{
	vec4 fit = coat.roughness * RESPONSE_FIT_SLOPE + RESPONSE_FIT_BASE;
	float grazing = (min(fit.x * fit.x, exp2(-RESPONSE_FIT_FALLOFF * facing)) * fit.x + fit.y) * RESPONSE_FIT_GRAZING;
	return coat.reflectance * (fit.z - grazing) + fit.w + grazing;
}

/* What a surface mirrors along a way: the sky above the horizon, and below it the ground, lost in the haze where it lies far off toward the horizon. */
vec3 Surroundings(vec3 way)
{
	return way.z > 0.0 ? SkyRadiance(way) : mix(Haze(way), GroundBounce(), smoothstep(0.0, 1.0, -way.z));
}

/* The sky and ground a surface reflects, blurring toward the light falling on it from all around the rougher it is. */
vec3 Sheen(vec3 normal, vec3 view, Coat coat, vec3 ambient)
{
	vec3 around = mix(Surroundings(reflect(-view, normal)), ambient, coat.roughness * coat.roughness);
	return around * SurroundingReflectance(coat, max(dot(normal, view), 0.0));
}

/* Left in the alpha of what the world draws, for the occlusion found over the screen to dim. */
float AmbientShare(vec3 ambient, vec3 sun)
{
	float around = Luminance(ambient);
	return around / (around + Luminance(sun));
}

/* The light a surface point's coat reflects toward the eye: the sun's highlight, and the sheen of its surroundings dimmed by occlusion. */
vec3 Reflected(vec3 normal, vec3 view, Coat coat, vec3 sun, vec3 ambient, float occlusion)
{
	return sun * Highlight(normal, view, SunDirection(), coat) + Sheen(normal, view, coat, ambient) * occlusion;
}

vec4 SurfaceRadiance(vec3 albedo, vec3 normal, vec3 view, Coat coat, float occlusion, vec3 sun, vec3 scattered_sun)
{
	vec3 ambient = AmbientLight(normal);
	vec3 scattered = Diffuse(albedo, scattered_sun + ambient * occlusion, coat.reflectance);
	return vec4(scattered + Reflected(normal, view, coat, sun, ambient, occlusion), AmbientShare(ambient, sun));
}

/* The light a surface point sends toward the eye with the share of the sun that reaches it given, and how brightly crystals in it glint in the sun. */
vec4 SunlitRadiance(vec3 albedo, vec3 normal, vec3 view, Coat coat, float occlusion, float glint, float sunlit)
{
	vec3 sun = Sunlight(normal, sunlit);
	return SurfaceRadiance(albedo, normal, view, coat, occlusion, sun, sun) + vec4(sun * glint, 0.0);
}

/* The light a surface point sends toward the eye, the sun's shut out where it is shadowed, with albedo in linear light and the point and normal in render space. */
vec4 Radiance(vec3 albedo, vec3 normal, vec3 position, Coat coat, float occlusion)
{
	return SunlitRadiance(albedo, normal, ToEye(position), coat, occlusion, 0.0, SunVisibility(position, normal));
}

vec3 Town(vec3 way)
{
	float sunward_walls = max(-dot(Across(way), SunDirection().xy), 0.0);
	return TOWN_ALBEDO * (SunRadiance() * sunward_walls + SKY_LIGHT);
}

vec3 PaneSurroundings(vec3 way, float height)
{
	if (way.z >= 0.0) return MirroredSky(way, PANE_SKY_LIFT);
	float reach = height * length(way.xy) / -way.z;
	vec3 sky = MirroredSky(vec3(way.xy, way.z * SKYLINE_FLATTENING), PANE_SKY_LIFT);
	vec3 skyline = mix(vec3(Luminance(sky)), sky, SKYLINE_SATURATION);
	return mix(Town(way), skyline, SKYLINE_SHARE * smoothstep(0.0, TOWN_REACH, reach));
}

vec4 PaneRadiance(Pane pane, vec3 normal, vec3 view, float roughness, float occlusion, float sunlit, float height)
{
	Coat coat = Coat(roughness, pane.reflectance);
	vec3 sun = Sunlight(normal, sunlit);
	vec3 ambient = AmbientLight(normal);
	float mirrored = SurroundingReflectance(coat, max(dot(normal, view), 0.0));
	vec3 through = Diffuse(pane.room, sun * ROOM_SUNLIT_SHARE + ambient * occlusion + LAMP_LIGHT * pane.lamp, mirrored);
	vec3 surroundings = PaneSurroundings(reflect(-view, normal), height) * mirrored * occlusion;
	return vec4(through + pane.film * (surroundings + sun * Highlight(normal, view, SunDirection(), coat)), AmbientShare(ambient, sun));
}

/* The sun through something thin: it wraps on past where the surface turns away from it, and glows through where it is seen against it. */
vec3 SunlightThrough(vec3 normal, vec3 view, float translucency, float sunlit)
{
	vec3 light = SunDirection();
	float wrapped = max(dot(normal, light) + translucency, 0.0) / (1.0 + translucency);
	float glow = translucency * pow(max(dot(-view, light), 0.0), GLOW_FOCUS_POWER);
	return SunRadiance() * (wrapped + glow) * sunlit;
}

/* Leaves let light through as well as reflecting it off their coat. */
vec4 FoliageRadiance(vec3 albedo, vec3 normal, vec3 view, float translucency, Coat coat, float occlusion, float sunlit)
{
	return SurfaceRadiance(albedo, normal, view, coat, occlusion, Sunlight(normal, sunlit), SunlightThrough(normal, view, translucency, sunlit));
}

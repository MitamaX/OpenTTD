const float PI = 3.14159265;

const vec3 ZENITH = vec3(0.08, 0.24, 0.70);
const vec3 HORIZON = vec3(0.45, 0.62, 0.90);
const vec3 SUN_GLOW = vec3(1.0, 0.70, 0.42);
const float SKY_BRIGHTNESS = 1.35;
const float SKY_CURVE = 0.45;
const float GLOW_FOCUS = 0.76;
const float GLOW_STRENGTH = 0.8;
const float HORIZON_GLOW = 0.25;
const float HORIZON_GLOW_FOCUS = 5.0;
const float HAZE_LIFT = 0.015;

const vec3 SUN_COLOUR = vec3(1.0, 0.87, 0.70);
const float SUN_ILLUMINANCE = 3.6;
const float SUN_DISC_COSINE = 0.99993;
const float SUN_DISC_EDGE = 0.99985;
const float SUN_DISC_BRIGHTNESS = 12.0;

const float HAZE_DENSITY = 0.0020;
const float HAZE_FALLOFF = 1.0 / 16.0;

vec3 SunRadiance()
{
	return SUN_COLOUR * SUN_ILLUMINANCE;
}

float HenyeyGreenstein(float cosine, float focus)
{
	float squared = focus * focus;
	return (1.0 - squared) / (4.0 * PI * pow(1.0 + squared - 2.0 * focus * cosine, 1.5));
}

/* The open sky above the horizon: deep blue overhead paling toward the horizon, warmed where it lies toward the sun. */
vec3 OpenSky(vec3 sight)
{
	float up = clamp(sight.z, 0.0, 1.0);
	vec3 gradient = mix(HORIZON, ZENITH, pow(up, SKY_CURVE));
	float facing = dot(sight, SunDirection());
	float low = 1.0 - up;
	float glow = GLOW_STRENGTH * HenyeyGreenstein(facing, GLOW_FOCUS) + HORIZON_GLOW * pow(max(facing, 0.0), HORIZON_GLOW_FOCUS) * low * low * low;
	return (gradient + SUN_GLOW * glow) * SKY_BRIGHTNESS;
}

/* The light the air scatters toward the eye along a sight line: the sky just over the horizon in that direction, so distant ground fades into it. */
vec3 Haze(vec3 sight)
{
	vec2 across = sight.xy / max(length(sight.xy), 1e-4);
	return OpenSky(normalize(vec3(across, HAZE_LIFT)));
}

vec3 SkyRadiance(vec3 sight)
{
	return sight.z > HAZE_LIFT ? OpenSky(sight) : Haze(sight);
}

vec3 SunDisc(vec3 sight)
{
	return SunRadiance() * SUN_DISC_BRIGHTNESS * smoothstep(SUN_DISC_EDGE, SUN_DISC_COSINE, dot(sight, SunDirection()));
}

/* The air thins with height, so how much of it a sight line crosses depends on the heights of both ends. */
float HazeDepth(vec3 from, vec3 to)
{
	float rise = (to.z - from.z) * HAZE_FALLOFF;
	float density = exp(-max(from.z, 0.0) * HAZE_FALLOFF);
	float mean = abs(rise) > 1e-3 ? density * (1.0 - exp(-rise)) / rise : density;
	return HAZE_DENSITY * length(to - from) * mean;
}

vec3 Hazed(vec3 radiance, vec3 from, vec3 to)
{
	return mix(Haze(normalize(to - from)), radiance, exp(-HazeDepth(from, to)));
}

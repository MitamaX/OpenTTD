const vec2 CLOUD_WIND = vec2(0.004, 0.0015);
const float CLOUD_SCALE = 1.6;
const float CLOUD_CURVE = 0.05;
const float CLOUD_COVER_FROM = 0.5;
const float CLOUD_COVER_TO = 0.78;
const float CLOUD_OPACITY = 0.85;
const float CLOUD_HORIZON_FADE = 0.1;
const int CLOUD_OCTAVES = 5;
const vec3 CLOUD_SHADE = vec3(0.66, 0.72, 0.84);
const vec3 CLOUD_LIT = vec3(1.0, 0.95, 0.88);
const float CLOUD_LIGHT_STEP = 0.08;
const float CLOUD_RELIEF = 6.0;
const float CLOUD_AMBIENT = 0.32;
const float CLOUD_SUNLIT = 0.34;
const float CLOUD_SILVER = 1.4;
const float CLOUD_SILVER_FOCUS = 0.6;

float Billow(vec2 p)
{
	float sum = 0.0;
	float weight = 0.5;
	for (int octave = 0; octave < CLOUD_OCTAVES; octave++) {
		sum += Noise(p) * weight;
		p = p * 2.03 + 11.7;
		weight *= 0.5;
	}
	return sum;
}

/* A thin layer of fair weather cloud drifting overhead, thinning out toward the horizon, lit on its tops and silvered toward the sun. */
vec3 Clouded(vec3 sky, vec3 sight)
{
	if (sight.z <= 0.0) return sky;
	vec2 p = sight.xy / (sight.z + CLOUD_CURVE) * CLOUD_SCALE + CLOUD_WIND * Clock();
	float body = Billow(p);
	float cover = smoothstep(CLOUD_COVER_FROM, CLOUD_COVER_TO, body) * smoothstep(0.0, CLOUD_HORIZON_FADE, sight.z) * CLOUD_OPACITY;
	float toward_sun = Billow(p + normalize(SunDirection().xy) * CLOUD_LIGHT_STEP);
	float sunlit = clamp(0.5 + (body - toward_sun) * CLOUD_RELIEF, 0.0, 1.0);
	vec3 lit = (CLOUD_SHADE * CLOUD_AMBIENT + CLOUD_LIT * CLOUD_SUNLIT * sunlit) * SunRadiance();
	lit += SunRadiance() * HenyeyGreenstein(dot(sight, SunDirection()), CLOUD_SILVER_FOCUS) * CLOUD_SILVER * (1.0 - cover);
	return mix(sky, lit, cover);
}

/* The sky as seen straight on, with its clouds; rippled water mirrors only the clear sky, as it would smear any cloud in it beyond telling. */
vec3 CloudedSky(vec3 sight)
{
	return sight.z > HAZE_LIFT ? Clouded(OpenSky(sight), sight) : Haze(sight);
}

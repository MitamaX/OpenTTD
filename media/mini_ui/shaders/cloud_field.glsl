const vec2 CLOUD_WIND = vec2(0.004, 0.0015);
const int CLOUD_SHADOW_OCTAVES = 3;
const float CLOUD_SHADOW_HEIGHT = 30.0;
const float CLOUD_SHADOW_SPAN = 24.0;
const float CLOUD_SHADOW_FROM = 0.36;
const float CLOUD_SHADOW_TO = 0.58;
const float CLOUD_SHADOW_DEPTH = 0.7;

float Billow(vec2 p, int octaves)
{
	float sum = 0.0;
	float weight = 0.5;
	for (int octave = 0; octave < octaves; octave++) {
		sum += Noise(p) * weight;
		p = p * 2.03 + 11.7;
		weight *= 0.5;
	}
	return sum;
}

/* The share of sunlight the drifting cloud layer lets through to a point, from the cloud the sun shines down through onto it;
 * the broad shapes alone cast it, so its edges fall soft. */
float CloudLight(vec3 position)
{
	vec3 sun = SunDirection();
	vec2 under = position.xy + sun.xy * (CLOUD_SHADOW_HEIGHT - position.z) / max(sun.z, 0.05);
	float body = Billow(under / CLOUD_SHADOW_SPAN + CLOUD_WIND * Clock(), CLOUD_SHADOW_OCTAVES);
	return 1.0 - CLOUD_SHADOW_DEPTH * smoothstep(CLOUD_SHADOW_FROM, CLOUD_SHADOW_TO, body);
}

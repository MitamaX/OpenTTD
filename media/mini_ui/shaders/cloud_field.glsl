const vec2 CLOUD_WIND = vec2(0.004, 0.0015);
const int CLOUD_SHADOW_OCTAVES = 3;
const float CLOUD_SHADOW_SPAN = 40.0;
const float CLOUD_SHADOW_FROM = 0.34;
const float CLOUD_SHADOW_TO = 0.62;
const float CLOUD_SHADOW_DEPTH = 0.3;
const float CLOUD_SHADOW_NEAR = 60.0;
const float CLOUD_SHADOW_FAR = 260.0;
const float CLOUD_SHADOW_FAR_DEPTH = 0.35;
const float CLOUD_SHADOW_HEIGHT = 30.0;

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

/* The cloud the sun shines through on its way to a point shades it. */
float CloudLight(vec3 position)
{
	vec2 under = position.xy + SunSlant() * max(CLOUD_SHADOW_HEIGHT - position.z, 0.0);
	float body = Billow(under / CLOUD_SHADOW_SPAN + CLOUD_WIND * Clock(), CLOUD_SHADOW_OCTAVES);
	float far = smoothstep(CLOUD_SHADOW_NEAR, CLOUD_SHADOW_FAR, distance(Eye(), position));
	return 1.0 - CLOUD_SHADOW_DEPTH * mix(1.0, CLOUD_SHADOW_FAR_DEPTH, far) * smoothstep(CLOUD_SHADOW_FROM, CLOUD_SHADOW_TO, body);
}

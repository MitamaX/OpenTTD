const float MATTE = 0.85;
const float POLISHED = 0.2;
const float GLOW_RADIANCE = 5.0;
const vec2 MIRROR_GLOSS = vec2(0.6, 0.8);

Coat CoatOf(float gloss)
{
	return Coat(mix(MATTE, POLISHED, gloss), mix(DIELECTRIC_REFLECTANCE, GLASS_REFLECTANCE, smoothstep(MIRROR_GLOSS.x, MIRROR_GLOSS.y, gloss)));
}

vec3 Glowing(vec3 albedo, float trait)
{
	return albedo * max(-trait, 0.0) * GLOW_RADIANCE;
}

const Fraying SHADOW_FRAYING = Fraying(9.0, 0.7, 0.6);
const float SHADOW_CANOPY_HOLES = 0.34;

float GapsAlongSun()
{
	vec3 sun = SunDirection();
	return Noise((v_position - sun * dot(v_position, sun)) * SHADOW_FRAYING.clumps_per_tile);
}

void main()
{
	float foliage = Foliage(SHADOW_FRAYING);
	if (foliage <= 0.0) return;
	float rim = FrayingReach(normalize(v_normal), SunDirection(), foliage, SHADOW_FRAYING);
	if (rim > 0.0) DropFrayed(rim, Clumps(foliage, SHADOW_FRAYING));
	DropFrayed(SHADOW_CANOPY_HOLES * foliage, GapsAlongSun());
}

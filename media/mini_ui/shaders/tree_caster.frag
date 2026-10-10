const Fraying SHADOW_FRAYING = Fraying(9.0, 0.7, 0.6);

void main()
{
	float foliage = Foliage(SHADOW_FRAYING);
	float reach = FrayingReach(normalize(v_normal), SunDirection(), foliage, SHADOW_FRAYING);
	if (reach > 0.0) DropFrayed(reach, Clumps(foliage, SHADOW_FRAYING));
}

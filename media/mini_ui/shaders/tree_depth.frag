void main()
{
	DropOutOfWindow();
	float foliage = Foliage(LEAF_FRAYING);
	DropFrayed(FrayingReach(normalize(v_normal), ToEye(v_position), foliage, LEAF_FRAYING), Clumps(foliage, LEAF_FRAYING));
}

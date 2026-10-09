void main()
{
	DropOutOfWindow();
	float foliage = Foliage();
	DropFrayed(normalize(v_normal), ToEye(v_position), Clumps(foliage), foliage);
}

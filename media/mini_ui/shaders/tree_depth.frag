void main()
{
	if (OutOfWindow()) discard;
	float foliage = Foliage();
	if (Frayed(normalize(v_normal), ToEye(v_position), Clumps(foliage), foliage)) discard;
}

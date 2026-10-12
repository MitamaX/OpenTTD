/* Buildings fade out dithered where they grow too small to show, and far off the sun's highlight spreads matte over a roof too small to make out. */
void main()
{
	tile_pixels = TilePixelsAt(v_position);
	DropUnseen(tile_pixels, v_surface.x, v_pattern);
	mat3 frame = FaceFrame();
	Clad clad = Worn(CladOf(v_surface.x, v_pattern, v_colour.rgb), frame);
	bool board = Has(SURFACE_EAVE);
	if (board || Has(SURFACE_ROOF)) clad = Snowed(clad, board ? RoofOverBoard(normalize(v_normal)) : frame[2]);
	frag_colour = CladRadiance(clad, Facing(clad, frame), frame);
}

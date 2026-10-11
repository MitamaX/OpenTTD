const float EDGE_ON_SHARE = 1e-4;

float PatternStretch(vec3 across)
{
	mat2x3 pixel = mat2x3(dFdx(v_position), dFdy(v_position));
	vec2 run = across * pixel;
	vec2 drawn = vec2(dFdx(v_pattern.x), dFdy(v_pattern.x));
	float spread = dot(run, run);
	bool edge_on = spread <= EDGE_ON_SHARE * (dot(pixel[0], pixel[0]) + dot(pixel[1], pixel[1]));
	return edge_on ? 1.0 : abs(dot(drawn, run)) / spread;
}

Outlook OutlookOf(mat3 frame)
{
	float stretch = PatternStretch(frame[0]);
	return Outlook(InTexels(ToEye(v_position) * frame, stretch), InTexels(SunDirection() * frame, stretch));
}

void main()
{
	tile_pixels = TilePixelsAt(v_position);
	DropUnseen(tile_pixels, v_surface.x, v_pattern);
	mat3 frame = FaceFrame();
	Clad wall = Worn(v_surface.x == CLAD_GLASS ? Spandrel(v_colour.rgb) : WallCladOf(v_surface.x, v_pattern, v_colour.rgb), frame);
	Clad clad = Facade(wall, v_surface.y, v_pattern, Has(SURFACE_FRONT), v_seed, OutlookOf(frame));
	vec3 normal = Facing(clad, frame);
	clad.albedo = Snowed(clad, normal).albedo;
	frag_colour = CladRadiance(clad, normal, frame);
}

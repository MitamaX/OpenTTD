in vec2 v_pattern;
flat in uvec4 v_surface;

/* Lattices cast the shadow of their members only. */
void main()
{
	if (v_surface.x == CLAD_LATTICE && LatticeCover(v_pattern) < ScreenNoise(gl_FragCoord.xy)) discard;
}

in vec2 v_pattern;
flat in uvec4 v_surface;

/* Open claddings cast the shadow of what they are made of only. */
void main()
{
	if (Cover(v_surface.x, v_pattern) < ScreenNoise(gl_FragCoord.xy)) discard;
}

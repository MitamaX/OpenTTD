in vec3 v_position;
in vec2 v_pattern;
flat in uvec4 v_surface;

void main()
{
	DropUnseen(TilePixelsAt(v_position), v_surface.x, v_pattern);
}

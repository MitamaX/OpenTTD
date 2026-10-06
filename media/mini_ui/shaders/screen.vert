uniform vec4 u_area;
uniform vec2 u_viewport;

out vec2 v_screen;

void main()
{
	vec2 corner = vec2(float(gl_VertexID & 1), float(gl_VertexID >> 1));
	v_screen = mix(u_area.xy, u_area.zw, corner);
	gl_Position = vec4(v_screen.x / u_viewport.x * 2.0 - 1.0, 1.0 - v_screen.y / u_viewport.y * 2.0, 0.0, 1.0);
}

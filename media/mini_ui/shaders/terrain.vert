layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_normal;

out vec3 v_world;
out vec3 v_normal;
out float v_wall;

void main()
{
	v_world = a_position;
	v_normal = a_normal.xyz;
	v_wall = a_normal.w;
	gl_Position = u_view_projection * vec4(RenderPoint(a_position), 1.0);
}

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_normal;
layout(location = 2) in vec4 a_colour;

out vec3 v_position;
out vec3 v_normal;
out vec4 v_colour;
out float v_trait;

void main()
{
	v_position = a_position;
	v_normal = a_normal.xyz;
	v_colour = a_colour;
	v_trait = a_normal.w;
	gl_Position = ClipPosition(a_position);
}

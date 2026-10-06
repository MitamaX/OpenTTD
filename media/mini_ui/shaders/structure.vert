layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_normal;
layout(location = 2) in vec4 a_colour;
layout(location = 3) in vec4 a_glass;
layout(location = 4) in vec2 a_pattern;
layout(location = 5) in vec4 a_surface;

out vec3 v_position;
out vec3 v_normal;
out vec4 v_colour;
out vec4 v_glass;
out vec2 v_pattern;
flat out uvec4 v_surface;

void main()
{
	v_position = a_position;
	v_normal = a_normal.xyz;
	v_colour = a_colour;
	v_glass = a_glass;
	v_pattern = a_pattern;
	v_surface = uvec4(round(a_surface * 255.0));
	gl_Position = ClipPosition(a_position);
}

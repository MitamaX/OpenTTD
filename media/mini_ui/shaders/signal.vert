layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_normal;
layout(location = 2) in vec4 a_colour;
layout(location = 3) in vec3 i_place;
layout(location = 4) in float i_facing;

out vec3 v_position;
out vec3 v_normal;
out vec4 v_colour;
out float v_trait;

void main()
{
	float c = cos(i_facing);
	float s = sin(i_facing);
	mat2 turn = mat2(c, s, -s, c);
	v_position = i_place + vec3(turn * a_position.xy, a_position.z);
	v_normal = vec3(turn * a_normal.xy, a_normal.z);
	v_colour = a_colour;
	v_trait = a_normal.w;
	gl_Position = ClipPosition(v_position);
}

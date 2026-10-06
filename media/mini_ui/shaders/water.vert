layout(location = 0) in vec3 a_position;

out vec3 v_world;
out vec3 v_position;

const float WATER_LIFT = 0.015;

void main()
{
	v_world = a_position;
	v_position = RenderPoint(a_position) + vec3(0.0, 0.0, WATER_LIFT);
	gl_Position = ClipPosition(v_position);
}

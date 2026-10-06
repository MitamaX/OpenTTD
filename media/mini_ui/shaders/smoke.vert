layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_normal;
layout(location = 2) in vec4 a_colour;
layout(location = 3) in vec3 i_vent;
layout(location = 4) in float i_radius;
layout(location = 5) in float i_phase;

out vec3 v_position;
out vec3 v_normal;
out vec4 v_colour;
out float v_age;

const float RISE_SECONDS = 9.0;
const float RISE_RADII = 22.0;
const float SPREAD = 3.2;
const vec2 WIND = vec2(0.55, -0.35);
const float DRIFT_RADII = 26.0;
const float TUMBLE = 0.7;

/* A puff swells as it rises and the wind bends its path ever more along, turning slowly as it goes. */
void main()
{
	float age = fract(Clock() / RISE_SECONDS + i_phase);
	float size = i_radius * (1.0 + SPREAD * sqrt(age));
	float angle = TUMBLE * (Clock() / RISE_SECONDS + i_phase * 6.2831853);
	mat2 turn = mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
	vec3 rise = vec3(WIND * DRIFT_RADII * age * age, RISE_RADII * age) * i_radius;
	v_position = i_vent + rise + vec3(turn * a_position.xy, a_position.z) * size;
	v_normal = vec3(turn * a_normal.xy, a_normal.z);
	v_colour = a_colour;
	v_age = age;
	gl_Position = ClipPosition(v_position);
}

layout(location = 0) in vec3 a_position;
layout(location = 3) in vec3 i_vent;
layout(location = 4) in float i_radius;
layout(location = 5) in float i_phase;
layout(location = 6) in vec2 i_motion;
layout(location = 7) in float i_seed;

out vec3 v_centre;
out vec2 v_corner;
out float v_size;
out float v_age;
out float v_seed;

const float RISE_SECONDS = 11.0;
const float RISE_RADII = 18.0;
const float DRIFT_RADII = 34.0;
const float FIRST_SIZE = 1.3;
const float LAST_SIZE = 7.5;
const float WOBBLE_RADII = 2.2;
const float WOBBLE_RATE = 0.6;
const float TAU = 6.2831853;

/* A puff rises fast out of its vent and slows as the wind takes it, bending the plume ever further along the way the clouds drift; it swells all the while.
 * A puff from a moving funnel stays where it was let go, so the plume trails behind. */
vec3 PuffCentre(float age)
{
	float seconds = age * RISE_SECONDS;
	vec2 wind = normalize(CLOUD_WIND);
	float lift = 1.0 - (1.0 - age) * (1.0 - age);
	float wobble = sin(Clock() * WOBBLE_RATE + i_seed * TAU) * WOBBLE_RADII * age;
	vec2 across = vec2(-wind.y, wind.x);
	vec2 drift = (wind * DRIFT_RADII * pow(age, 1.5) + across * wobble) * i_radius - i_motion * seconds;
	return i_vent + vec3(drift, RISE_RADII * lift * i_radius);
}

void main()
{
	float age = fract(Clock() / RISE_SECONDS + i_phase);
	vec3 centre = PuffCentre(age);
	float size = i_radius * mix(FIRST_SIZE, LAST_SIZE, sqrt(age)) * (0.85 + 0.3 * i_seed);
	vec3 right = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
	vec3 up = vec3(u_view[0][1], u_view[1][1], u_view[2][1]);
	v_centre = centre;
	v_corner = a_position.xy;
	v_size = size;
	v_age = age;
	v_seed = i_seed;
	gl_Position = ClipPosition(centre + (right * a_position.x + up * a_position.y) * size);
}

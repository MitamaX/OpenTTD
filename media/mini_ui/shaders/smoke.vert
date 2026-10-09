layout(location = 0) in vec3 a_position;
layout(location = 3) in vec3 i_vent;
layout(location = 4) in float i_radius;
layout(location = 5) in float i_phase;
layout(location = 6) in vec2 i_motion;
layout(location = 7) in float i_seed;
layout(location = 8) in float i_spray;

out vec3 v_centre;
out vec2 v_corner;
out float v_size;
out float v_age;
out float v_seed;
out float v_cloud_light;
out float v_spray;

const float RISE_SECONDS = 11.0;
const float RISE_RADII = 18.0;
const float DRIFT_RADII = 34.0;
const vec2 SIZES = vec2(1.3, 7.5);
const float SPRAY_SECONDS = 3.5;
const float SPRAY_RISE_RADII = 2.5;
const float SPRAY_WIND_RADII = 1.0;
const vec2 SPRAY_SIZES = vec2(1.0, 3.0);
const float WOBBLE_RADII = 2.2;
const float WOBBLE_RATE = 0.6;
const float TAU = 6.2831853;

/* A puff rises fast out of its vent and slows as the wind takes it, bending the plume ever further along the way the clouds drift; it swells all the while.
 * A puff from a moving funnel stays where it was let go, so the plume trails behind, as spray off a fall is carried off downstream. Spray rises low and briefly. */
vec3 PuffCentre(float age, float rise_seconds)
{
	float seconds = age * rise_seconds;
	vec2 wind = normalize(CLOUD_WIND);
	float lift = 1.0 - (1.0 - age) * (1.0 - age);
	float wobble = sin(Clock() * WOBBLE_RATE + i_seed * TAU) * WOBBLE_RADII * age;
	vec2 across = vec2(-wind.y, wind.x);
	vec2 drift = (wind * mix(DRIFT_RADII, SPRAY_WIND_RADII, i_spray) * pow(age, 1.5) + across * wobble) * i_radius - i_motion * seconds;
	return i_vent + vec3(drift, mix(RISE_RADII, SPRAY_RISE_RADII, i_spray) * lift * i_radius);
}

void main()
{
	float rise_seconds = mix(RISE_SECONDS, SPRAY_SECONDS, i_spray);
	float age = fract(Clock() / rise_seconds + i_phase);
	vec3 centre = PuffCentre(age, rise_seconds);
	vec2 sizes = mix(SIZES, SPRAY_SIZES, i_spray);
	float size = i_radius * mix(sizes.x, sizes.y, sqrt(age)) * (0.85 + 0.3 * i_seed);
	vec3 right = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
	vec3 up = vec3(u_view[0][1], u_view[1][1], u_view[2][1]);
	v_centre = centre;
	v_corner = a_position.xy;
	v_size = size;
	v_age = age;
	v_seed = i_seed;
	v_cloud_light = CloudLight(centre);
	v_spray = i_spray;
	gl_Position = ClipPosition(centre + (right * a_position.x + up * a_position.y) * size);
}

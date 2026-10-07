layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_normal;
layout(location = 2) in vec4 a_colour;
layout(location = 3) in vec3 i_place;
layout(location = 4) in vec3 i_attitude;
layout(location = 5) in float i_length;
layout(location = 6) in vec4 i_primary;
layout(location = 7) in vec4 i_secondary;
layout(location = 8) in vec4 i_cargo;

uniform vec2 u_most_growth;
uniform float u_readable_pixels;

out vec3 v_position;
out vec3 v_normal;
out vec4 v_colour;
out float v_trait;

const float KEY_FULL = 254.5 / 255.0;
const float KEY_NONE = 0.5 / 255.0;

vec3 Yawed(vec3 p, float angle)
{
	float c = cos(angle);
	float s = sin(angle);
	return vec3(p.x * c - p.y * s, p.x * s + p.y * c, p.z);
}

vec3 Pitched(vec3 p, float angle)
{
	float c = cos(angle);
	float s = sin(angle);
	return vec3(p.x * c - p.z * s, p.y, p.x * s + p.z * c);
}

vec3 Rolled(vec3 p, float angle)
{
	float c = cos(angle);
	float s = sin(angle);
	return vec3(p.x, p.y * c - p.z * s, p.y * s + p.z * c);
}

vec3 Turned(vec3 p)
{
	return Yawed(Pitched(Rolled(p, i_attitude.z), i_attitude.y), i_attitude.x);
}

bool IsCargo(vec3 tone)
{
	return tone.r < KEY_NONE && tone.g < KEY_NONE && tone.b > KEY_NONE;
}

/* Paintwork tones take the owner's colours or the cargo's, shaded by their blue up to twice as bright. */
vec3 Painted(vec3 tone)
{
	float shade = tone.b * 2.0;
	if (tone.r > KEY_FULL && tone.g < KEY_NONE) return min(i_primary.rgb * shade, 1.0);
	if (tone.r < KEY_NONE && tone.g > KEY_FULL) return min(i_secondary.rgb * shade, 1.0);
	if (IsCargo(tone)) return min(i_cargo.rgb * shade, 1.0);
	return tone;
}

/* Far off a vehicle grows, up to its most, so it still reads where a tile spans only a few pixels. */
vec3 Growth()
{
	float growth = max(u_readable_pixels / TilePixelsAt(distance(Eye(), i_place)), 1.0);
	return vec3(min(growth, u_most_growth.x), vec2(min(growth, u_most_growth.y)));
}

/* Cargo shows only while the unit carries some; empty, it folds away to nothing. */
void main()
{
	vec3 scale = vec3(i_length, 1.0, 1.0) * Growth();
	vec3 local = a_position * scale;
	if (IsCargo(a_colour.rgb) && i_cargo.a <= 0.0) local = vec3(0.0);
	v_position = i_place + Turned(local);
	v_normal = Turned(normalize(a_normal.xyz / scale));
	v_colour = vec4(Painted(a_colour.rgb), a_colour.a);
	v_trait = a_normal.w;
	gl_Position = ClipPosition(v_position);
}

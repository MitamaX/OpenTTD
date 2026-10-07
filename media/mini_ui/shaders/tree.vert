uniform int u_detail;

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_normal;
layout(location = 2) in vec4 a_colour;
layout(location = 3) in vec3 i_place;
layout(location = 4) in vec4 i_look;

out vec3 v_position;
out vec3 v_normal;
out vec3 v_colour;
out float v_openness;
out float v_translucency;
out float v_tint;
out float v_wither;
out float v_snow;
flat out vec2 v_window;

const float TAU = 6.2831853;
const vec2 WIND_WAY = vec2(0.8, 0.6);
const float WIND_REACH = 0.014;
const float GUST_RATE = 1.3;
const float FLUTTER_RATE = 2.9;
const float FLUTTER_SHARE = 0.3;
const float CROWDED_SHADE = 0.6;
const vec4 HIDDEN = vec4(2.0, 2.0, 2.0, 1.0);

mat2 Turn(float angle)
{
	float c = cos(angle);
	float s = sin(angle);
	return mat2(c, s, -s, c);
}

/* Each tree sways on its own beat, its crown more than its foot. */
vec2 Sway(float height, float scale)
{
	float lift = clamp(height / TREE_SWAY_HEIGHT, 0.0, 1.0);
	float beat = Hash(ivec2(floor(i_place.xy * 64.0))) * TAU;
	float swing = mix(sin(Clock() * GUST_RATE + beat), sin(Clock() * FLUTTER_RATE + beat * 1.7), FLUTTER_SHARE);
	return WIND_WAY * swing * WIND_REACH * lift * lift * scale;
}

float SnowOn(vec3 place)
{
	if (Landscape() != LANDSCAPE_ARCTIC) return 0.0;
	return smoothstep(0.3, 0.8, ClimateAt(place.xy).blanket + SnowLift(place.z));
}

/* Trees deep in a forest are shaded underneath by their neighbours. */
void ReadGround()
{
	uvec4 codes = texelFetch(u_tiles, Clamped(ivec2(floor(i_place.xy))), 0);
	float crowd = FloraCover(codes);
	v_openness = a_colour.a * (1.0 - CROWDED_SHADE * crowd * (1.0 - a_colour.a));
	v_snow = SnowOn(i_place);
}

/* A copy outside its detail's window collapses to a point off screen before any of it is drawn. */
void main()
{
	vec3 base = RenderPoint(i_place);
	v_window = DetailWindow(TilePixelsAt(length(Eye() - base)), u_detail);
	if (v_window.x >= v_window.y) {
		gl_Position = HIDDEN;
		return;
	}

	float scale = i_look.y * TREE_LARGEST_SCALE;
	mat2 turn = Turn(i_look.x * TAU);
	vec3 local = a_position * scale;
	local.xy = turn * local.xy + Sway(a_position.z, scale);
	v_position = base + local;
	v_normal = vec3(turn * a_normal.xy, a_normal.z);
	v_colour = a_colour.rgb;
	v_translucency = a_normal.w;
	v_tint = i_look.z;
	v_wither = i_look.w;
#ifdef SHADOW_CASTER
	v_openness = a_colour.a;
	v_snow = 0.0;
#else
	ReadGround();
#endif
	gl_Position = ClipPosition(v_position);
}

uniform int u_detail;

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_normal;
layout(location = 2) in vec4 a_colour;
layout(location = 3) in vec3 i_place;
layout(location = 4) in vec4 i_look;

out vec3 v_position;
out vec3 v_normal;
out vec3 v_colour;
out vec3 v_grown;
out float v_openness;
out float v_reach;
out float v_translucency;
out float v_tint;
out float v_hue;
out float v_wither;
out float v_snow;
flat out vec2 v_window;
flat out float v_crown_sunlit;

invariant gl_Position;

const float TAU = 6.2831853;
const vec2 WIND_WAY = vec2(0.8, 0.6);
const float GUST_RATE = 1.3;
const float FLUTTER_RATE = 2.9;
const float FLUTTER_SHARE = 0.3;
const float CROWDED_SHADE = 0.6;
const vec4 HIDDEN = vec4(2.0, 2.0, 2.0, 1.0);
const float SEATS_PER_TILE = 64.0;
const ivec2 GROWTH_SALT = ivec2(7919, 104729);
const float THICKET_FREQUENCY = 0.45;
const float STAND_FREQUENCY = 0.3;
const float STAND_SHARE = 0.6;
const float GRAIN_OFFSET = 37.0;

/* How a tree grows its own way: turned, leaning a little off upright, stretched taller or squatter, and sized. */
struct Growth {
	mat2 turn;
	vec2 lean;
	float stretch;
	float scale;
};

mat2 Turn(float angle)
{
	float c = cos(angle);
	float s = sin(angle);
	return mat2(c, s, -s, c);
}

/* The cell a tree stands in, finer than any two trees stand apart, which its own dice are rolled from. */
ivec2 Seat()
{
	return ivec2(floor(i_place.xy * SEATS_PER_TILE));
}

/* Each tree grows its own way by its own dice, and taller in the thickets that run across the tiles than between them. */
Growth GrowthOf(ivec2 seat)
{
	float thicket = mix(TREE_THICKET_SCALES[0], TREE_THICKET_SCALES[1], Noise(i_place.xy * THICKET_FREQUENCY));
	vec2 lean = (Hash2(seat + GROWTH_SALT) - 0.5) * 2.0 * TREE_LEAN;
	float stretch = mix(TREE_STRETCHES[0], TREE_STRETCHES[1], Hash(seat - GROWTH_SALT));
	return Growth(Turn(i_look.x * TAU), lean, stretch, i_look.y * TREE_LARGEST_SCALE * thicket);
}

vec3 Grown(Growth growth, vec3 point)
{
	return vec3(growth.turn * point.xy + growth.lean * point.z, point.z * growth.stretch) * growth.scale;
}

/* A normal carried through the growth, square to the surface as it is grown. */
vec3 GrownNormal(Growth growth, vec3 normal)
{
	vec2 turned = growth.turn * normal.xy;
	return vec3(turned, (normal.z - dot(growth.lean, turned)) / growth.stretch);
}

/* Each tree sways on its own beat, its crown more than its foot. */
vec2 Sway(float height, float scale, float beat)
{
	float lift = clamp(height / TREE_SWAY_HEIGHT, 0.0, 1.0);
	float swing = mix(sin(Clock() * GUST_RATE + beat), sin(Clock() * FLUTTER_RATE + beat * 1.7), FLUTTER_SHARE);
	return WIND_WAY * swing * TREE_SWAY_REACH * lift * lift * scale;
}

/* The trees of a stand share much of their hue, and each its own share besides. */
float HueOf(ivec2 seat)
{
	return mix(Hash(seat + GROWTH_SALT.yx), Noise(i_place.xy * STAND_FREQUENCY + 5.1), STAND_SHARE);
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
	v_window = DetailWindow(TilePixelsAt(base), u_detail);
	if (v_window.x >= v_window.y) {
		gl_Position = HIDDEN;
		return;
	}

	ivec2 seat = Seat();
	float dice = Hash(seat);
	Growth growth = GrowthOf(seat);
	vec3 local = Grown(growth, a_position);
	v_grown = local + dice * GRAIN_OFFSET;
	local.xy += Sway(a_position.z, growth.scale, dice * TAU);
	v_position = base + local;
	v_normal = GrownNormal(growth, a_normal.xyz);
	v_colour = a_colour.rgb;
	v_reach = a_colour.a;
	v_translucency = a_normal.w;
	v_tint = i_look.z;
	v_wither = i_look.w;
#ifdef SHADOW_CASTER
	v_openness = a_colour.a;
	v_snow = 0.0;
#else
	v_hue = HueOf(seat);
	v_crown_sunlit = SunReaches(base + Grown(growth, vec3(0.0, 0.0, TREE_MODEL_TALLEST)));
	ReadGround();
#endif
	gl_Position = ClipPosition(v_position);
}

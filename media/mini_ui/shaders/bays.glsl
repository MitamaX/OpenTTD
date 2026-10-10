const uint OPEN_NONE = 0u;
const uint OPEN_WINDOW = 1u;
const uint OPEN_SHOP = 2u;
const uint OPEN_DOOR = 3u;
const uint OPEN_GLASS_DOOR = 4u;
const uint OPEN_SHUTTER = 5u;

const uint MUNTIN_NONE = 0u;
const uint MUNTIN_CROSS = 1u;
const uint MUNTIN_SASH = 2u;
const uint MUNTIN_MULLION = 3u;
const uint MUNTIN_TRANSOM = 4u;
const uint MUNTIN_GRID = 5u;

/* An opening in texels: what fills it, how wide and tall it is, how far above its band it starts, its glazing bars and whether it is arched. */
struct Opening {
	uint kind;
	float width;
	float height;
	float sill;
	uint muntin;
	bool arch;
};

const Opening NO_OPENING = Opening(OPEN_NONE, 0.0, 0.0, 0.0, MUNTIN_NONE, false);
const Opening COTTAGE_WINDOW = Opening(OPEN_WINDOW, 7.0, 4.0, 2.0, MUNTIN_CROSS, false);
const Opening COTTAGE_DOOR = Opening(OPEN_DOOR, 5.0, 7.0, 0.0, MUNTIN_NONE, false);
const Opening TERRACE_WINDOW = Opening(OPEN_WINDOW, 4.0, 5.0, 1.0, MUNTIN_SASH, false);
const Opening TERRACE_DOOR = Opening(OPEN_DOOR, 4.0, 6.0, 0.0, MUNTIN_NONE, false);
const Opening FLAT_WINDOW = Opening(OPEN_WINDOW, 6.0, 4.0, 2.0, MUNTIN_TRANSOM, false);
const Opening FLAT_ENTRANCE = Opening(OPEN_GLASS_DOOR, 6.0, 7.0, 0.0, MUNTIN_NONE, false);
const Opening SHOP_UPPER_WINDOW = Opening(OPEN_WINDOW, 5.0, 4.0, 2.0, MUNTIN_MULLION, false);
const Opening SHOP_DISPLAY = Opening(OPEN_SHOP, 12.0, 7.0, 2.0, MUNTIN_NONE, false);
const Opening SHOP_DOOR = Opening(OPEN_GLASS_DOOR, 4.0, 9.0, 0.0, MUNTIN_NONE, false);
const Opening OFFICE_RIBBON = Opening(OPEN_WINDOW, 7.0, 3.0, 3.0, MUNTIN_NONE, false);
const Opening OFFICE_LOBBY = Opening(OPEN_WINDOW, 7.0, 8.0, 2.0, MUNTIN_TRANSOM, false);
const Opening OFFICE_ENTRANCE = Opening(OPEN_GLASS_DOOR, 7.0, 10.0, 0.0, MUNTIN_MULLION, false);
const Opening CURTAIN_PANEL = Opening(OPEN_WINDOW, 7.4, 6.0, 1.0, MUNTIN_NONE, false);
const Opening CURTAIN_LOBBY = Opening(OPEN_WINDOW, 7.4, 10.0, 1.0, MUNTIN_TRANSOM, false);
const Opening CURTAIN_ENTRANCE = Opening(OPEN_GLASS_DOOR, 7.4, 11.0, 0.0, MUNTIN_MULLION, false);
const Opening ARCHED_WINDOW = Opening(OPEN_WINDOW, 6.0, 10.0, 3.0, MUNTIN_SASH, true);
const Opening ARCHED_DOOR = Opening(OPEN_DOOR, 8.0, 13.0, 0.0, MUNTIN_MULLION, true);
const Opening FACTORY_WINDOW = Opening(OPEN_WINDOW, 11.0, 7.0, 5.0, MUNTIN_GRID, false);
const Opening CLERESTORY = Opening(OPEN_WINDOW, 22.0, 4.0, 6.0, MUNTIN_GRID, false);
const Opening LOADING_SHUTTER = Opening(OPEN_SHUTTER, 20.0, 13.0, 0.0, MUNTIN_NONE, false);
const Opening BAY_SHUTTER = Opening(OPEN_SHUTTER, 24.0, 14.0, 0.0, MUNTIN_NONE, false);

/* Indexed by window grid. */
const Opening UPPER_OPENINGS[WINDOW_GRIDS] = Opening[WINDOW_GRIDS](NO_OPENING, COTTAGE_WINDOW, TERRACE_WINDOW, FLAT_WINDOW, SHOP_UPPER_WINDOW, OFFICE_RIBBON, CURTAIN_PANEL, ARCHED_WINDOW, FACTORY_WINDOW, CLERESTORY);
const Opening GROUND_OPENINGS[WINDOW_GRIDS] = Opening[WINDOW_GRIDS](NO_OPENING, COTTAGE_WINDOW, TERRACE_WINDOW, FLAT_WINDOW, SHOP_DISPLAY, OFFICE_LOBBY, CURTAIN_LOBBY, ARCHED_WINDOW, FACTORY_WINDOW, BAY_SHUTTER);
const Opening ENTRANCES[WINDOW_GRIDS] = Opening[WINDOW_GRIDS](NO_OPENING, COTTAGE_DOOR, TERRACE_DOOR, FLAT_ENTRANCE, SHOP_DOOR, OFFICE_ENTRANCE, CURTAIN_ENTRANCE, ARCHED_DOOR, LOADING_SHUTTER, NO_OPENING);

const float SHADOW_SOFTNESS = 0.3;
const float TOY_ROUNDING = 0.6;

const ivec2 WINDOW_SALT = ivec2(7, 13);
const int ROLL_STRIDE = 7919;
const int STOREY_COLUMN = -4099;
const int ROLL_TONE = 0;
const int ROLL_LAMP = 1;
const int ROLL_BLIND = 2;
const int ROLL_CURTAIN = 3;
const int ROLL_BLIND_PAINT = 4;
const int ROLL_DEPTH = 6;
const int ROLL_CURTAIN_PAINT = 7;
const int ROLL_WALL_PAINT = 8;
const int ROLL_FLOOR_PAINT = 9;
const int ROLL_LAMP_PAINT = 10;
const int ROLL_KIND = 11;
const int ROLL_FITTING = 12;
const int ROLL_PIECE = 13;

/* Where a point of a facade lies: the opening of its bay, the point measured from that opening's foot at its middle, and which window it is. */
struct Bay {
	Opening opening;
	vec2 at;
	ivec2 window;
};

struct Outlook {
	vec3 eye;
	vec3 sun;
};

struct Elevation {
	uint grid;
	uint seed;
	bool front;
	Outlook outlook;
	float pixel;
};

Bay BayAt(uint grid, vec2 texel, bool front)
{
	float ground = FACADE_GROUND[grid];
	float storey = FACADE_STOREY[grid];
	float pitch = FACADE_PITCH[grid];
	float column = floor(texel.x / pitch);
	float across = texel.x - (column + 0.5) * pitch;
	if (texel.y >= ground) {
		float level = floor((texel.y - ground) / storey);
		Opening upper = UPPER_OPENINGS[grid];
		return Bay(upper, vec2(across, texel.y - ground - level * storey - upper.sill), ivec2(column, level + 1.0));
	}

	Opening entrance = ENTRANCES[grid];
	float from_entrance = mod(texel.x, TEXELS) - FACADE_BAY[grid] * 0.5;
	if (front && entrance.kind != OPEN_NONE && abs(from_entrance) < entrance.width * 0.5 + 1.0) return Bay(entrance, vec2(from_entrance, texel.y), ivec2(-1, 0));
	Opening opening = GROUND_OPENINGS[grid];
	return Bay(opening, vec2(across, texel.y - opening.sill), ivec2(column, 0));
}

vec3 InTexels(vec3 way, float stretch)
{
	return vec3(way.x * stretch, way.y / FORM_HEIGHT_SCALE, way.z);
}

vec2 SightShift(Outlook outlook)
{
	return -Slant(outlook.eye);
}

vec2 SideOf(vec2 v)
{
	return mix(vec2(-1.0), vec2(1.0), step(0.0, v));
}

vec2 Heading(vec2 shift)
{
	return SideOf(shift) * max(abs(shift), vec2(LEAST_DIVISOR));
}

vec2 ReachOut(vec2 from, vec2 heading, vec2 low, vec2 high)
{
	return (mix(low, high, step(0.0, heading)) - from) / heading;
}

vec2 ReachIn(vec2 from, vec2 heading, vec2 low, vec2 high)
{
	return ReachOut(from, heading, high, low);
}

float ToyCornerRadius(vec2 half_size)
{
	return min(half_size.x, half_size.y) * TOY_ROUNDING;
}

vec3 RoundedEdge(vec2 half_size, vec2 from_centre)
{
	float radius = ToyCornerRadius(half_size);
	vec2 corner = abs(from_centre) - half_size + radius;
	vec2 rounding = max(corner, 0.0);
	float rounded = length(rounding);
	float outside = rounded + min(max(corner.x, corner.y), 0.0) - radius;
	vec2 outward = rounded > 0.0 ? rounding / rounded : corner.x > corner.y ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
	return vec3(outward * SideOf(from_centre), outside);
}

/* Which way out of the opening its nearest edge faces, and how far outside it a point lies in texels, negative inside. */
vec3 NearestEdge(Opening opening, vec2 at)
{
	float half_width = opening.width * 0.5;
	if (Landscape() == LANDSCAPE_TOYLAND) return RoundedEdge(vec2(half_width, opening.height * 0.5), at - vec2(0.0, opening.height * 0.5));
	float spring = opening.height - half_width;
	if (opening.arch && at.y > spring) {
		vec2 from_centre = vec2(at.x, at.y - spring);
		float reach = length(from_centre);
		return reach - half_width >= -at.y ? vec3(from_centre / reach, reach - half_width) : vec3(0.0, -1.0, -at.y);
	}
	vec3 sides = vec3(abs(at.x) - half_width, -at.y, at.y - opening.height);
	if (sides.x >= max(sides.y, sides.z)) return vec3(SideOf(at).x, 0.0, sides.x);
	return sides.y >= sides.z ? vec3(0.0, -1.0, sides.y) : vec3(0.0, 1.0, sides.z);
}

float OutsideOpening(Opening opening, vec2 at)
{
	return NearestEdge(opening, at).z;
}

float SunThrough(Opening opening, vec2 at, float sunk, vec3 sun)
{
	if (sun.z <= 0.0) return 0.0;
	vec2 mouth = at + Slant(sun) * sunk;
	return 1.0 - smoothstep(-SHADOW_SOFTNESS, SHADOW_SOFTNESS, OutsideOpening(opening, mouth));
}

bool OnMuntin(Opening opening, vec2 at, float pixel)
{
	float bar = max(0.3, pixel);
	float middle = opening.height * 0.5;
	switch (opening.muntin) {
		case MUNTIN_CROSS: return abs(at.x) < bar || abs(at.y - middle) < bar;
		case MUNTIN_SASH: return abs(at.y - middle) < bar;
		case MUNTIN_MULLION: return abs(at.x) < bar;
		case MUNTIN_TRANSOM: return abs(at.y - (opening.height - 2.0)) < bar;
		case MUNTIN_GRID: return abs(fract((at.x + opening.width * 0.5) / 4.0 + 0.5) - 0.5) * 4.0 < bar || abs(fract(at.y / 4.0 + 0.5) - 0.5) * 4.0 < bar;
		default: return false;
	}
}

ivec2 Die(ivec2 window, uint seed, int roll)
{
	return window * WINDOW_SALT + ivec2(int(seed), roll * ROLL_STRIDE);
}

float Roll(ivec2 window, uint seed, int roll)
{
	return Hash(Die(window, seed, roll));
}

uint Picked(ivec2 window, uint seed, int roll, int count)
{
	return Pick(Roll(window, seed, roll), count);
}

float Chosen(float rolled, float share, vec2 span)
{
	return rolled < share ? mix(span.x, span.y, rolled / share) : 0.0;
}

bool HoldsOffices(uint grid)
{
	return grid == WINDOWS_OFFICE || grid == WINDOWS_CURTAIN;
}

bool Mullioned(uint grid)
{
	return grid == WINDOWS_CURTAIN;
}

bool HoldsHomes(uint grid)
{
	return grid == WINDOWS_COTTAGE || grid == WINDOWS_TERRACE || grid == WINDOWS_APARTMENT;
}

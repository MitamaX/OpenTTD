const float SHADOW_SOFTNESS = 0.3;
const float TOY_ROUNDING = 0.6;
const vec2 TOY_WIDTHS = vec2(0.7, 1.0);
const vec2 TOY_HEIGHTS = vec2(0.75, 1.15);
const float TOY_ROUND_SHARE = 0.25;
const float TOY_FRAME_ROOM = 1.0;
const vec2 TOY_STRETCH = vec2(1.0, FORM_HEIGHT_SCALE);

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
const int ROLL_PIECE_CHOICE = 15;
const int ROLL_FITTING_KIND = 17;
const int ROLL_PARTITION = 18;
const int ROLL_TOY_SHAPE = 19;

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

Opening ToyShaped(Opening opening, ivec2 window, uint seed, float pitch, float band)
{
	if (Landscape() != LANDSCAPE_TOYLAND || opening.kind == OPEN_NONE) return opening;
	float middle = opening.sill + opening.height * 0.5;
	float width = min(opening.width * mix(TOY_WIDTHS.x, TOY_WIDTHS.y, Roll(ivec2(window.x, STOREY_COLUMN), seed, ROLL_TOY_SHAPE)), pitch - TOY_FRAME_ROOM);
	float height = min(opening.height * mix(TOY_HEIGHTS.x, TOY_HEIGHTS.y, Roll(ivec2(STOREY_COLUMN, window.y), seed, ROLL_TOY_SHAPE)), 2.0 * min(middle, band - middle) - TOY_FRAME_ROOM);
	float diameter = min(width, height * FORM_HEIGHT_SCALE);
	opening.rounded = Roll(window, seed, ROLL_TOY_SHAPE) < TOY_ROUND_SHARE;
	opening.width = opening.rounded ? diameter : width;
	opening.height = opening.rounded ? diameter / FORM_HEIGHT_SCALE : height;
	opening.sill = middle - opening.height * 0.5;
	return opening;
}

Bay BayAt(Elevation elevation, vec2 texel)
{
	uint grid = elevation.grid;
	float ground = FACADE_GROUND[grid];
	float storey = FACADE_STOREY[grid];
	float pitch = FACADE_PITCH[grid];
	float column = floor(texel.x / pitch);
	float across = texel.x - (column + 0.5) * pitch;
	bool upper = texel.y >= ground;
	Opening entrance = ENTRANCES[grid];
	float from_entrance = mod(texel.x, TEXELS) - FACADE_BAY[grid] * 0.5;
	if (!upper && elevation.front && entrance.kind != OPEN_NONE && abs(from_entrance) < entrance.width * 0.5 + 1.0) return Bay(entrance, vec2(from_entrance, texel.y), ivec2(-1, 0));

	float level = upper ? floor((texel.y - ground) / storey) : -1.0;
	float foot = upper ? ground + level * storey : 0.0;
	ivec2 window = ivec2(column, level + 1.0);
	Opening opening = ToyShaped(upper ? UPPER_OPENINGS[grid] : GROUND_OPENINGS[grid], window, elevation.seed, pitch, upper ? storey : ground);
	return Bay(opening, vec2(across, texel.y - foot - opening.sill), window);
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

vec2 ToyHalfSize(Opening opening)
{
	return vec2(opening.width, opening.height * FORM_HEIGHT_SCALE) * 0.5;
}

float ToyCornerRadius(vec2 half_size, bool full)
{
	return min(half_size.x, half_size.y) * (full ? 1.0 : TOY_ROUNDING);
}

vec3 RoundedEdge(vec2 half_size, vec2 from_centre, bool full)
{
	float radius = ToyCornerRadius(half_size, full);
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
	if (Landscape() == LANDSCAPE_TOYLAND) return RoundedEdge(ToyHalfSize(opening), (at - vec2(0.0, opening.height * 0.5)) * TOY_STRETCH, opening.rounded);
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

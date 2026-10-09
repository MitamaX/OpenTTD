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
const Opening LOADING_SHUTTER = Opening(OPEN_SHUTTER, 20.0, 13.0, 0.0, MUNTIN_NONE, false);
const Opening BAY_SHUTTER = Opening(OPEN_SHUTTER, 24.0, 14.0, 0.0, MUNTIN_NONE, false);

/* Indexed by window grid. */
const Opening UPPER_OPENINGS[10] = Opening[10](NO_OPENING, COTTAGE_WINDOW, TERRACE_WINDOW, FLAT_WINDOW, SHOP_UPPER_WINDOW, OFFICE_RIBBON, CURTAIN_PANEL, ARCHED_WINDOW, FACTORY_WINDOW, NO_OPENING);
const Opening GROUND_OPENINGS[10] = Opening[10](NO_OPENING, COTTAGE_WINDOW, TERRACE_WINDOW, FLAT_WINDOW, SHOP_DISPLAY, OFFICE_LOBBY, CURTAIN_LOBBY, ARCHED_WINDOW, FACTORY_WINDOW, BAY_SHUTTER);
const Opening ENTRANCES[10] = Opening[10](NO_OPENING, COTTAGE_DOOR, TERRACE_DOOR, FLAT_ENTRANCE, SHOP_DOOR, OFFICE_ENTRANCE, CURTAIN_ENTRANCE, ARCHED_DOOR, LOADING_SHUTTER, NO_OPENING);
const float FASCIA_ROWS[10] = float[10](0.0, 0.0, 0.0, 0.0, 2.0, 0.0, 0.0, 0.0, 0.0, 0.0);

const vec3 FRAME_PAINTS[] = FRAME_TONES;
const vec3 MULLION_PAINTS[] = MULLION_TONES;
const vec3 SILL_STONE = vec3(0.80, 0.78, 0.74);
const float SURROUND_SHARE = 0.55;
const float SURROUND_WIDTH = 1.0;
const float SURROUND_BEVEL = 0.5;
const float MULLION_WIDTH = 0.3;
const float SASH_WIDTH = 0.4;
const float SHOPFRONT_RIM_WIDTH = 0.6;
const float SILL_HEIGHT = 1.0;
const float SILL_OVERHANG = 1.0;
const float SILL_LEDGE_TILT = 1.2;
const float SILL_PROJECTION = 0.8;
const float UNDER_SILL_OPENNESS = 0.8;
const vec3 SHOPFRONT_FRAME = vec3(0.22);
const vec3 SPANDREL_BACKING = vec3(0.34, 0.35, 0.36);
const vec3 SHUTTER_SLAT = vec3(0.62, 0.64, 0.66);
const vec3 SHUTTER_GROOVE = vec3(0.50, 0.52, 0.54);
const float SLATS_PER_TEXEL = 0.5;
const float SHUTTER_HOOD_SHADE = 0.8;
const float DOOR_PANEL_SHADE = 0.7;
const float PAINT_ROUGHNESS = 0.6;
const float FRAME_ROUGHNESS = 0.5;
const float SHUTTER_ROUGHNESS = 0.45;
const float STONE_ROUGHNESS = 0.7;
const vec3 DOOR_PAINTS[5] = vec3[5](vec3(0.42, 0.12, 0.10), vec3(0.12, 0.28, 0.18), vec3(0.12, 0.20, 0.36), vec3(0.30, 0.20, 0.12), vec3(0.85, 0.84, 0.80));
const vec3 FASCIA_PAINTS[4] = vec3[4](vec3(0.55, 0.12, 0.10), vec3(0.10, 0.24, 0.40), vec3(0.14, 0.32, 0.18), vec3(0.62, 0.48, 0.16));
const float DRIP_REACH = 5.0;
const float DRIP_SHADE = 0.86;
const float DRIPS_PER_TEXEL = 0.5;
const vec2 DRIP_EDGES = vec2(0.25, 0.85);
const float DRIP_VARIETY = 0.8;
const vec2 DRIP_WINDOW_SEED = vec2(7.0, 13.0);

const float RECESS_DEPTH = 1.5;
const float CURTAIN_RECESS = 0.3;
const float RECESS_OPENNESS = 0.55;
const float LOWEST_FACING = 0.05;
const float SHADOW_SOFTNESS = 0.3;
const float LEAST_PIXEL = 1e-3;

const ivec2 WINDOW_SALT = ivec2(7, 13);
const int ROLL_STRIDE = 7919;
const int STOREY_COLUMN = -4099;
const int ROLL_TONE = 0;
const int ROLL_LAMP = 1;
const int ROLL_BLIND = 2;
const int ROLL_CURTAIN = 3;
const int ROLL_BLIND_PAINT = 4;
const int ROLL_WARP = 5;
const int ROLL_DEPTH = 6;
const int ROLL_CURTAIN_PAINT = 7;
const int ROLL_WALL_PAINT = 8;
const int ROLL_FLOOR_PAINT = 9;

const vec2 PANE_WARP = vec2(0.03, 0.08);
const float PANE_ROUGHNESS = 0.06;
const float GLAZED_SHARE = 0.85;
const vec2 ROOM_TONES = vec2(0.45, 1.15);
const float LIT_ROOM_SHARE = 0.08;
const float LIT_SHOP_SHARE = 0.6;
const float LIT_STOREY_SHARE = 0.18;
const float LIT_IN_LIT_STOREY = 0.75;
const vec3 BLINDS[3] = vec3[3](vec3(0.88, 0.87, 0.84), vec3(0.58, 0.58, 0.56), vec3(0.86, 0.80, 0.66));
const uint OFFICE_BLINDS = 2u;
const float BLINDED_STOREY_SHARE = 0.3;
const float BLINDED_ROOM_SHARE = 0.22;
const vec2 BLIND_DROPS = vec2(0.15, 0.85);
const vec3 CURTAINS[4] = vec3[4](vec3(0.62, 0.20, 0.16), vec3(0.78, 0.66, 0.42), vec3(0.30, 0.42, 0.30), vec3(0.70, 0.62, 0.58));
const float CURTAINED_ROOM_SHARE = 0.35;
const vec2 CURTAIN_REACHES = vec2(0.1, 0.32);
const vec3 ROOM_WALLS[4] = vec3[4](vec3(0.78, 0.74, 0.66), vec3(0.70, 0.72, 0.68), vec3(0.80, 0.75, 0.68), vec3(0.64, 0.67, 0.72));
const vec3 ROOM_FLOORS[3] = vec3[3](vec3(0.44, 0.31, 0.20), vec3(0.38, 0.36, 0.34), vec3(0.54, 0.42, 0.30));
const vec3 OFFICE_WALL = vec3(0.80, 0.80, 0.78);
const vec3 OFFICE_FLOOR = vec3(0.36, 0.37, 0.39);
const vec3 ROOM_CEILING = vec3(0.84, 0.84, 0.82);
const vec3 FURNITURE = vec3(0.26, 0.23, 0.21);
const vec2 HOME_DEPTHS = vec2(5.0, 10.0);
const vec2 OFFICE_DEPTHS = vec2(9.0, 16.0);
const float ROOM_PARTITION = 0.5;
const float FLOOR_SLAB = 1.0;
const float ROOM_SIDE_SHADE = 0.8;
const float ROOM_FAR_LIGHT = 0.22;
const float FURNITURE_HEIGHT = 2.0;
const float FURNITURE_PER_TEXEL = 0.45;
const float FURNITURE_SEED = 17.0;
const float FURNITURE_GAPS = 0.45;
const vec2 ROOM_RESOLVED_PIXELS = vec2(0.5, 1.2);

const float ARCTIC_LAMPS = 2.5;
const float TROPIC_ROOM_SHADE = 0.7;
const vec3 LOUVRE_PAINTS[4] = vec3[4](vec3(0.20, 0.42, 0.30), vec3(0.22, 0.38, 0.55), vec3(0.18, 0.50, 0.50), vec3(0.86, 0.84, 0.78));
const float LOUVRE_GROOVE_SHADE = 0.72;
const float TOY_ROUNDING = 0.6;
const vec3 TOY_GLASS = vec3(0.62, 0.86, 0.96);
const vec3 TOY_FRAME = vec3(0.95, 0.95, 0.93);
const float TOY_PANE_ROUGHNESS = 0.3;
const float TOY_GLAZING = 0.45;

const vec2 COLUMNS_FADE = vec2(1.2, 2.6);
const vec2 STOREYS_FADE = vec2(2.6, 4.5);

/* Where a point of a facade lies: the opening of its bay, the point measured from that opening's foot at its middle, and which window it is. */
struct Bay {
	Opening opening;
	vec2 at;
	ivec2 window;
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

struct Outlook {
	vec3 eye;
	vec3 sun;
};

vec3 InTexels(vec3 way)
{
	return vec3(way.x, way.y / FORM_HEIGHT_SCALE, way.z);
}

vec2 SightShift(Outlook outlook)
{
	return -outlook.eye.xy / max(outlook.eye.z, LOWEST_FACING);
}

vec2 SideOf(vec2 v)
{
	return mix(vec2(-1.0), vec2(1.0), step(0.0, v));
}

vec2 Heading(vec2 shift)
{
	return SideOf(shift) * max(abs(shift), vec2(MIN_SPREAD));
}

vec2 ReachOut(vec2 from, vec2 heading, vec2 low, vec2 high)
{
	return (mix(low, high, step(0.0, heading)) - from) / heading;
}

vec3 RoundedEdge(vec2 half_size, vec2 from_centre)
{
	float radius = min(half_size.x, half_size.y) * TOY_ROUNDING;
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

struct Room {
	float tone;
	float lamp;
	float drop;
	vec3 blind;
	float drawn;
	vec3 curtain;
	float depth;
	vec3 wall;
	vec3 floor;
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

float Chosen(float die, float share, vec2 span)
{
	return die < share ? mix(span.x, span.y, die / share) : 0.0;
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

vec3 FramePaint(uint grid, uint seed)
{
	if (Landscape() == LANDSCAPE_TOYLAND) return TOY_FRAME;
	return Mullioned(grid) ? MULLION_PAINTS[seed % uint(MULLION_PAINTS.length())] : FRAME_PAINTS[seed % uint(FRAME_PAINTS.length())];
}

Clad FrameOf(uint grid, uint seed)
{
	return Matte(FramePaint(grid, seed), FRAME_ROUGHNESS);
}

float LampShare(float share)
{
	return Landscape() == LANDSCAPE_ARCTIC ? min(share * ARCTIC_LAMPS, 1.0) : share;
}

bool InGroove(float y)
{
	return fract(y * SLATS_PER_TEXEL) < 0.5;
}

Room RoomBehind(Bay bay, uint grid, uint seed)
{
	ivec2 storey = ivec2(STOREY_COLUMN, bay.window.y);
	bool offices = HoldsOffices(grid);
	bool lit_storey = offices && Roll(storey, seed, ROLL_LAMP) < LIT_STOREY_SHARE;
	float lit = LampShare(bay.opening.kind == OPEN_SHOP ? LIT_SHOP_SHARE : lit_storey ? LIT_IN_LIT_STOREY : LIT_ROOM_SHARE);
	vec2 depths = offices ? OFFICE_DEPTHS : HOME_DEPTHS;
	Room room;
	room.tone = mix(ROOM_TONES.x, ROOM_TONES.y, Roll(bay.window, seed, ROLL_TONE)) * (Landscape() == LANDSCAPE_TROPIC ? TROPIC_ROOM_SHADE : 1.0);
	room.lamp = Roll(bay.window, seed, ROLL_LAMP) < lit ? 1.0 : 0.0;
	room.drop = offices ? Chosen(Roll(storey, seed, ROLL_BLIND), BLINDED_STOREY_SHARE, BLIND_DROPS) : Chosen(Roll(bay.window, seed, ROLL_BLIND), BLINDED_ROOM_SHARE, BLIND_DROPS);
	room.blind = BLINDS[offices ? seed % OFFICE_BLINDS : Picked(bay.window, seed, ROLL_BLIND_PAINT, BLINDS.length())];
	room.drawn = offices ? 0.0 : Chosen(Roll(bay.window, seed, ROLL_CURTAIN), CURTAINED_ROOM_SHARE, CURTAIN_REACHES);
	room.curtain = CURTAINS[Picked(bay.window, seed, ROLL_CURTAIN_PAINT, CURTAINS.length())];
	room.depth = mix(depths.x, depths.y, Roll(bay.window, seed, ROLL_DEPTH));
	room.wall = offices ? OFFICE_WALL : ROOM_WALLS[Picked(bay.window, seed, ROLL_WALL_PAINT, ROOM_WALLS.length())];
	room.floor = offices ? OFFICE_FLOOR : ROOM_FLOORS[Picked(bay.window, seed, ROLL_FLOOR_PAINT, ROOM_FLOORS.length())];
	return room;
}

vec3 Furnished(Room room, vec2 hit, float floor)
{
	bool furniture = hit.y - floor < FURNITURE_HEIGHT && Noise(vec2(hit.x * FURNITURE_PER_TEXEL, room.tone * FURNITURE_SEED)) > FURNITURE_GAPS;
	return furniture ? FURNITURE : room.wall;
}

vec3 RoomSeen(Room room, Bay bay, uint grid, Outlook outlook)
{
	vec2 shift = SightShift(outlook);
	vec2 heading = Heading(shift);
	float half_width = FACADE_PITCH[grid] * 0.5 - ROOM_PARTITION;
	float floor = -bay.opening.sill;
	float ceiling = floor + (bay.window.y == 0 ? FACADE_GROUND[grid] : FACADE_STOREY[grid]) - FLOOR_SLAB;
	vec2 reach = ReachOut(bay.at, heading, vec2(-half_width, floor), vec2(half_width, ceiling));
	float sunk = min(room.depth, min(reach.x, reach.y));
	vec2 hit = bay.at + shift * sunk;
	vec3 flat_surface = heading.y > 0.0 ? ROOM_CEILING : room.floor;
	vec3 surface = sunk >= room.depth ? Furnished(room, hit, floor) : reach.y < reach.x ? flat_surface : room.wall * ROOM_SIDE_SHADE;
	float daylight = mix(1.0, ROOM_FAR_LIGHT, sunk / OFFICE_DEPTHS.y);
	return surface * room.tone * mix(daylight, 1.0, room.lamp);
}

Clad PaneOver(vec3 room, float lamp)
{
	return Clad(room, FACING_OUT, PANE_ROUGHNESS, 1.0, 1.0, 1.0, lamp);
}

Clad ToyPane()
{
	Clad pane = Matte(TOY_GLASS, TOY_PANE_ROUGHNESS);
	pane.glass = TOY_GLAZING;
	return pane;
}

Clad MeanGlazing()
{
	return Landscape() == LANDSCAPE_TOYLAND ? ToyPane() : PaneOver(DIM_ROOM, LampShare(LIT_ROOM_SHARE));
}

vec3 SeenThrough(Room room, Bay bay, uint grid, Outlook outlook, float pixel)
{
	Opening opening = bay.opening;
	if (bay.at.y > opening.height * (1.0 - room.drop)) return room.blind;
	if (abs(bay.at.x) > opening.width * (0.5 - room.drawn)) return room.curtain;
	float resolved = 1.0 - smoothstep(ROOM_RESOLVED_PIXELS.x, ROOM_RESOLVED_PIXELS.y, pixel);
	vec3 dim = DIM_ROOM * room.tone;
	return resolved > 0.0 ? mix(dim, RoomSeen(room, bay, grid, outlook), resolved) : dim;
}

Clad Glazing(Bay bay, uint grid, uint seed, Outlook outlook, float pixel)
{
	if (Landscape() == LANDSCAPE_TOYLAND) return ToyPane();
	Room room = RoomBehind(bay, grid, seed);
	Clad clad = PaneOver(SeenThrough(room, bay, grid, outlook, pixel), room.lamp);
	clad.facing = vec3((Hash2(Die(bay.window, seed, ROLL_WARP)) - 0.5) * PANE_WARP, 1.0);
	return clad;
}

/* What fills an opening: glass behind its bars and frame, a painted door, or the slats of a shutter. */
Clad Filling(Bay bay, uint grid, uint seed, Outlook outlook, float pixel)
{
	Opening opening = bay.opening;
	vec2 at = bay.at;
	if (opening.kind == OPEN_DOOR) {
		vec3 paint = DOOR_PAINTS[(seed + uint(bay.window.y)) % uint(DOOR_PAINTS.length())];
		return Matte(paint * (OnMuntin(opening, at, pixel) ? DOOR_PANEL_SHADE : 1.0), PAINT_ROUGHNESS);
	}
	if (opening.kind == OPEN_SHUTTER) return Matte(at.y > opening.height - 1.0 ? SHUTTER_GROOVE * SHUTTER_HOOD_SHADE : InGroove(at.y) ? SHUTTER_GROOVE : SHUTTER_SLAT, SHUTTER_ROUGHNESS);
	bool sashed = opening.kind == OPEN_WINDOW;
	bool held_by_mullions = sashed && Mullioned(grid);
	bool rimmed = !held_by_mullions && OutsideOpening(opening, at) > -max(sashed ? SASH_WIDTH : SHOPFRONT_RIM_WIDTH, pixel);
	if (rimmed || OnMuntin(opening, at, pixel)) return sashed ? FrameOf(grid, seed) : Matte(SHOPFRONT_FRAME, FRAME_ROUGHNESS);
	return Glazing(bay, grid, seed, outlook, pixel);
}

float InsideShare(float outside, float pixel)
{
	return clamp(0.5 - outside / max(pixel, LEAST_PIXEL), 0.0, 1.0);
}

float RecessOpenness(float sunk)
{
	return mix(1.0, RECESS_OPENNESS, sunk / RECESS_DEPTH);
}

float SunThrough(Opening opening, vec2 at, float sunk, vec3 sun)
{
	if (sun.z <= 0.0) return 0.0;
	vec2 mouth = at + sun.xy * (sunk / max(sun.z, LOWEST_FACING));
	return 1.0 - smoothstep(-SHADOW_SOFTNESS, SHADOW_SOFTNESS, OutsideOpening(opening, mouth));
}

Clad Reveal(Clad wall, Opening opening, vec2 at, float depth, Outlook outlook)
{
	vec2 shift = SightShift(outlook);
	vec2 heading = Heading(shift);
	vec2 reach = ReachOut(at, heading, vec2(-opening.width * 0.5, 0.0), vec2(opening.width * 0.5, opening.height));
	float sunk = clamp(min(reach.x, reach.y), 0.0, depth);
	vec2 hit = at + shift * sunk;
	Clad reveal = Matte(wall.albedo, wall.roughness);
	reveal.facing = vec3(-NearestEdge(opening, hit).xy, 0.0);
	reveal.occlusion = RecessOpenness(sunk);
	reveal.sunlit = SunThrough(opening, hit, sunk, outlook.sun);
	return reveal;
}

Clad Recessed(Clad wall, Bay bay, uint grid, uint seed, Outlook outlook, float pixel)
{
	Opening opening = bay.opening;
	float depth = Mullioned(grid) ? CURTAIN_RECESS : RECESS_DEPTH;
	Bay sunk = Bay(opening, bay.at + SightShift(outlook) * depth, bay.window);
	Clad filling = Filling(sunk, grid, seed, outlook, pixel);
	filling.occlusion = RecessOpenness(depth);
	filling.sunlit = SunThrough(opening, sunk.at, depth, outlook.sun);
	float behind = InsideShare(OutsideOpening(opening, sunk.at), pixel);
	if (behind >= 1.0) return filling;
	Clad jamb = Mullioned(grid) ? FrameOf(grid, seed) : wall;
	return Mixed(Reveal(jamb, opening, bay.at, depth, outlook), filling, behind);
}

bool BesideWindow(Opening opening, vec2 at)
{
	float half_width = opening.width * 0.5;
	return at.y >= 0.0 && at.y <= opening.height && abs(at.x) >= half_width && abs(at.x) <= opening.width;
}

Clad Louvre(vec2 at, uint seed)
{
	vec3 paint = LOUVRE_PAINTS[seed % uint(LOUVRE_PAINTS.length())];
	return Matte(InGroove(at.y) ? paint * LOUVRE_GROOVE_SHADE : paint, PAINT_ROUGHNESS);
}

bool AlongSill(Opening opening, vec2 at)
{
	return abs(at.x) < opening.width * 0.5 + SILL_OVERHANG;
}

Clad UnderSill(Clad wall, Opening opening, vec2 at, vec3 sun)
{
	float below = -SILL_HEIGHT - at.y;
	if (below < 0.0 || !AlongSill(opening, at) || sun.y <= 0.0) return wall;
	float shadow = SILL_PROJECTION * sun.y / max(sun.z, LOWEST_FACING);
	wall.sunlit *= smoothstep(shadow - SHADOW_SOFTNESS, shadow + SHADOW_SOFTNESS, below);
	wall.occlusion *= mix(UNDER_SILL_OPENNESS, 1.0, smoothstep(0.0, 1.0, below));
	return wall;
}

float Drips(Bay bay, vec2 texel, uint grid)
{
	if (texel.y < FACADE_GROUND[grid] || bay.opening.kind == OPEN_NONE) return 1.0;
	float below = mod(-bay.at.y, FACADE_STOREY[grid]);
	float half_width = bay.opening.width * 0.5;
	float under = 1.0 - smoothstep(half_width - 0.5, half_width + 0.5, abs(bay.at.x));
	ivec2 shedding = bay.at.y < 0.0 ? bay.window : bay.window + ivec2(0, 1);
	float runs = smoothstep(DRIP_EDGES.x, DRIP_EDGES.y, Noise(vec2(bay.at.x * DRIPS_PER_TEXEL, dot(vec2(shedding), DRIP_WINDOW_SEED))));
	float fade = 1.0 - smoothstep(0.0, DRIP_REACH * Varied(runs, DRIP_VARIETY), below);
	return mix(1.0, DRIP_SHADE, under * runs * fade);
}

Clad FacadeWall(uint material, vec2 pattern, vec3 tint)
{
	if (material != CLAD_GLASS) return CladOf(material, pattern, tint);
	return Landscape() == LANDSCAPE_TOYLAND ? Matte(tint, TOY_PANE_ROUGHNESS) : PaneOver(SPANDREL_BACKING, 0.0);
}

Clad Windowed(Clad wall, uint grid, vec2 texel, bool front, uint seed, Outlook outlook, float pixel)
{
	Bay bay = BayAt(grid, texel, front);
	Opening opening = bay.opening;
	Clad clad = wall;
	clad.albedo *= Drips(bay, texel, grid);

	float fascia = FASCIA_ROWS[grid];
	float ground = FACADE_GROUND[grid];
	if (front && fascia > 0.0 && texel.y < ground && texel.y >= ground - 1.0 - fascia) {
		clad = texel.y >= ground - 1.0 ? Matte(SILL_STONE, STONE_ROUGHNESS) : Matte(FASCIA_PAINTS[seed % uint(FASCIA_PAINTS.length())], PAINT_ROUGHNESS);
	}
	if (opening.kind == OPEN_NONE) return clad;

	vec3 edge = NearestEdge(opening, bay.at);
	bool sills = !Mullioned(grid) && (opening.kind == OPEN_WINDOW || opening.kind == OPEN_SHOP);
	float springline = opening.arch ? opening.height - opening.width * 0.5 : opening.height;
	bool framed = edge.z < (Mullioned(grid) ? MULLION_WIDTH : SURROUND_WIDTH) && (!sills || bay.at.y >= springline);
	bool on_sill = sills && bay.at.y < 0.0 && bay.at.y >= -SILL_HEIGHT && AlongSill(opening, bay.at);
	bool louvred = Landscape() == LANDSCAPE_TROPIC && HoldsHomes(grid) && opening.kind == OPEN_WINDOW && BesideWindow(opening, bay.at);
	if (sills) clad = UnderSill(clad, opening, bay.at, outlook.sun);
	if (on_sill) {
		clad = Matte(SILL_STONE, STONE_ROUGHNESS);
		clad.facing = vec3(0.0, SILL_LEDGE_TILT, 1.0);
	} else if (louvred) {
		clad = Louvre(bay.at, seed);
	} else if (framed) {
		clad = Mullioned(grid) ? FrameOf(grid, seed) : Matte(mix(wall.albedo, SILL_STONE, SURROUND_SHARE), STONE_ROUGHNESS);
		clad.facing = vec3(-edge.xy * SURROUND_BEVEL, 1.0);
	}
	float inside = InsideShare(edge.z, pixel);
	if (inside <= 0.0) return clad;
	return Mixed(clad, Recessed(wall, bay, grid, seed, outlook, pixel), inside);
}

Clad Banded(Clad wall, Clad glazed, uint grid, float height, float pixel)
{
	float ground = FACADE_GROUND[grid];
	float storey = FACADE_STOREY[grid];
	Opening opening = height < ground ? GROUND_OPENINGS[grid] : UPPER_OPENINGS[grid];
	float within = height < ground ? height : mod(height - ground, storey);
	float across = opening.width / FACADE_PITCH[grid];
	return Mixed(wall, glazed, Band(within, opening.sill, opening.sill + opening.height, pixel) * across * GLAZED_SHARE);
}

/* A facade's windows, doors and frames over its wall, settling from afar into its storeys' bands of glass and then into one glazed wall. */
Clad Facade(Clad wall, uint grid, vec2 pattern, bool front, uint seed, Outlook outlook)
{
	vec2 texel = pattern * TEXELS;
	vec2 pixel = vec2(fwidth(texel.x), fwidth(texel.y));
	float columns = 1.0 - smoothstep(COLUMNS_FADE.x, COLUMNS_FADE.y, max(pixel.x, pixel.y));
	float storeys = 1.0 - smoothstep(STOREYS_FADE.x, STOREYS_FADE.y, pixel.y);
	Opening upper = UPPER_OPENINGS[grid];
	Clad glazed = MeanGlazing();
	Clad far = Mixed(wall, glazed, upper.width * upper.height / (FACADE_PITCH[grid] * FACADE_STOREY[grid]) * GLAZED_SHARE);
	if (storeys <= 0.0) return far;
	Clad banded = Mixed(far, Banded(wall, glazed, grid, texel.y, pixel.y), storeys);
	if (columns <= 0.0) return banded;
	return Mixed(banded, Windowed(wall, grid, texel, front, seed, outlook, max(pixel.x, pixel.y)), columns);
}

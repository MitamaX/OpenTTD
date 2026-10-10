const float PANE_ROUGHNESS = 0.06;

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
const float CURTAIN_SIDES = 2.0;
const vec3 ROOM_WALLS[4] = vec3[4](vec3(0.78, 0.74, 0.66), vec3(0.70, 0.72, 0.68), vec3(0.80, 0.75, 0.68), vec3(0.64, 0.67, 0.72));
const vec3 ROOM_FLOORS[3] = vec3[3](vec3(0.44, 0.31, 0.20), vec3(0.38, 0.36, 0.34), vec3(0.54, 0.42, 0.30));
const vec3 OFFICE_WALL = vec3(0.80, 0.80, 0.78);
const vec3 OFFICE_FLOOR = vec3(0.36, 0.37, 0.39);
const vec3 ROOM_CEILING = vec3(0.84, 0.84, 0.82);
const vec3 FURNITURE = vec3(0.26, 0.23, 0.21);
const vec3 PALE_WOOD = vec3(0.58, 0.44, 0.30);
const vec3 DESK_TOP = vec3(0.78, 0.78, 0.76);
const vec3 STEEL_GREY = vec3(0.46, 0.48, 0.50);
const vec3 FOLIAGE = vec3(0.20, 0.38, 0.16);
const vec2 HOME_DEPTHS = vec2(5.0, 10.0);
const vec2 OFFICE_DEPTHS = vec2(9.0, 16.0);
const float DEEPEST_ROOM = OFFICE_DEPTHS.y;
const float ROOM_PARTITION = 0.5;
const float FLOOR_SLAB = 1.0;
const float ROOM_SIDE_SHADE = 0.8;
const float ROOM_FAR_LIGHT = 0.22;
const float PIECE_TOP_LIGHT = 1.2;
const float SUNBEAM_LIGHT = 0.6;
const float SUNLIT_ROOM_SHARE = 0.2;
const float MEAN_SUNBEAM = SUNBEAM_LIGHT * SUNLIT_ROOM_SHARE;
const vec2 ROOM_RESOLVED_PIXELS = vec2(0.5, 1.2);
const vec3 HOME_LAMPS[2] = vec3[2](vec3(1.0, 0.76, 0.50), vec3(1.0, 0.83, 0.60));
const vec3 OFFICE_LAMPS[2] = vec3[2](vec3(1.0, 0.90, 0.76), vec3(1.0, 0.95, 0.86));
const float LAMP_LIGHT = 1.5;
const float LAMP_REACH = 8.0;
const float LAMP_DEPTH_SHARE = 0.45;
const float CEILING_LAMP_GLOW = 4.0;
const float OFFICE_PANEL_PITCH = 4.0;
const float PANEL_HALF_LENGTH = 0.6;
const float PANEL_SPREAD = 0.6;
const float HOME_FIXTURE_RADIUS = 0.8;
const float DRAPE_GLOW = 0.4;
const float ROOM_MEAN_DAYLIGHT = 0.6;
const float ROOM_MEAN_LAMP = 0.5;
const float ROOM_MEAN_FLAT_SHARE = 0.5;
const vec3 TYPICAL_LAMP = mix(HOME_LAMPS[0], OFFICE_LAMPS[0], 0.5) * LAMP_LIGHT * ROOM_MEAN_LAMP;

const vec3 GOODS[6] = vec3[6](vec3(0.78, 0.22, 0.18), vec3(0.20, 0.42, 0.70), vec3(0.88, 0.74, 0.22), vec3(0.24, 0.56, 0.32), vec3(0.86, 0.84, 0.80), vec3(0.62, 0.36, 0.62));
const vec3 SHELF_PAINT = vec3(0.32, 0.30, 0.28);
const float SHELF_PITCH = 2.0;
const float SHELF_BOARD = 0.15;
const float GOODS_PITCH = 0.6;
const float GOODS_GAPS = 0.2;
const ivec2 GOODS_SALT = ivec2(31, 57);

const float ARCTIC_LAMPS = 2.5;
const float TROPIC_ROOM_SHADE = 0.7;

const uint ROOM_LIVING = 0u;
const uint ROOM_BEDROOM = 1u;
const uint ROOM_OFFICE = 2u;
const uint ROOM_MEETING = 3u;
const uint ROOM_SHOP = 4u;
const int ROOM_KINDS = 5;
const float BEDROOM_SHARE = 0.4;
const float MEETING_ROOM_SHARE = 0.25;

struct Piece {
	vec3 size;
	vec2 set_back;
	vec3 paint;
	bool dyed;
};

const Piece SOFA = Piece(vec3(4.5, 1.6, 2.2), vec2(0.2, 0.7), vec3(0.0), true);
const Piece BOOKCASE = Piece(vec3(2.2, 4.6, 0.8), vec2(1.0), FURNITURE, false);
const Piece BED = Piece(vec3(4.2, 1.3, 5.0), vec2(0.4, 1.0), vec3(0.0), true);
const Piece WARDROBE = Piece(vec3(3.0, 5.0, 1.4), vec2(1.0), PALE_WOOD, false);
const Piece DESKS = Piece(vec3(5.0, 1.8, 1.8), vec2(0.0, 0.6), DESK_TOP, false);
const Piece CABINETS = Piece(vec3(2.6, 2.8, 1.0), vec2(1.0), STEEL_GREY, false);
const Piece TABLE = Piece(vec3(5.0, 1.6, 3.2), vec2(0.2, 0.7), FURNITURE, false);
const Piece PLANTER = Piece(vec3(1.4, 3.2, 1.4), vec2(0.6, 1.0), FOLIAGE, false);
const Piece COUNTER = Piece(vec3(6.0, 2.0, 1.4), vec2(0.1, 0.5), PALE_WOOD, false);
const Piece RACK = Piece(vec3(2.4, 3.0, 2.0), vec2(0.4, 0.9), vec3(0.0), true);
const int PIECES_PER_ROOM = 2;
const Piece PIECES[ROOM_KINDS * PIECES_PER_ROOM] = Piece[ROOM_KINDS * PIECES_PER_ROOM](SOFA, BOOKCASE, BED, WARDROBE, DESKS, CABINETS, TABLE, PLANTER, COUNTER, RACK);

struct Fitting {
	vec2 size;
	float raised;
	vec3 paint;
};

const Fitting DOORWAY = Fitting(vec2(2.6, 5.4), 0.0, vec3(0.30, 0.22, 0.16));
const Fitting PICTURE = Fitting(vec2(2.6, 1.8), 2.6, vec3(0.36, 0.44, 0.52));
const Fitting SCREEN = Fitting(vec2(4.0, 2.2), 2.0, vec3(0.06, 0.07, 0.08));
const Fitting BARE_WALL = Fitting(vec2(0.0), 0.0, vec3(0.0));
const Fitting BACK_FITTINGS[ROOM_KINDS] = Fitting[ROOM_KINDS](PICTURE, DOORWAY, DOORWAY, SCREEN, BARE_WALL);

struct Room {
	ivec2 window;
	uint seed;
	bool offices;
	uint kind;
	float tone;
	vec3 lamp;
	float drop;
	float drawn;
};

float LampShare(float share)
{
	return Landscape() == LANDSCAPE_ARCTIC ? min(share * ARCTIC_LAMPS, 1.0) : share;
}

vec3 LampPaint(ivec2 window, uint seed, bool offices)
{
	float roll = Roll(window, seed, ROLL_LAMP_PAINT);
	return offices ? OFFICE_LAMPS[Pick(roll, OFFICE_LAMPS.length())] : HOME_LAMPS[Pick(roll, HOME_LAMPS.length())];
}

uint RoomKind(Bay bay, uint seed, bool offices)
{
	if (bay.opening.kind == OPEN_SHOP) return ROOM_SHOP;
	float roll = Roll(bay.window, seed, ROLL_KIND);
	if (offices) return roll < MEETING_ROOM_SHARE ? ROOM_MEETING : ROOM_OFFICE;
	return roll < BEDROOM_SHARE ? ROOM_BEDROOM : ROOM_LIVING;
}

Room RoomBehind(Bay bay, Elevation elevation)
{
	uint seed = elevation.seed;
	ivec2 storey = ivec2(STOREY_COLUMN, bay.window.y);
	bool offices = HoldsOffices(elevation.grid);
	bool lit_storey = offices && Roll(storey, seed, ROLL_LAMP) < LIT_STOREY_SHARE;
	float lit = LampShare(bay.opening.kind == OPEN_SHOP ? LIT_SHOP_SHARE : lit_storey ? LIT_IN_LIT_STOREY : LIT_ROOM_SHARE);
	Room room;
	room.window = bay.window;
	room.seed = seed;
	room.offices = offices;
	room.kind = RoomKind(bay, seed, offices);
	room.tone = mix(ROOM_TONES.x, ROOM_TONES.y, Roll(bay.window, seed, ROLL_TONE)) * (Landscape() == LANDSCAPE_TROPIC ? TROPIC_ROOM_SHADE : 1.0);
	room.lamp = Roll(bay.window, seed, ROLL_LAMP) < lit ? LampPaint(bay.window, seed, offices) * LAMP_LIGHT : vec3(0.0);
	room.drop = offices ? Chosen(Roll(storey, seed, ROLL_BLIND), BLINDED_STOREY_SHARE, BLIND_DROPS) : Chosen(Roll(bay.window, seed, ROLL_BLIND), BLINDED_ROOM_SHARE, BLIND_DROPS);
	room.drawn = offices ? 0.0 : Chosen(Roll(bay.window, seed, ROLL_CURTAIN), CURTAINED_ROOM_SHARE, CURTAIN_REACHES);
	return room;
}

float RoomDepth(Room room)
{
	vec2 depths = room.offices ? OFFICE_DEPTHS : HOME_DEPTHS;
	return mix(depths.x, depths.y, Roll(room.window, room.seed, ROLL_DEPTH));
}

vec3 RoomWall(Room room)
{
	return room.offices ? OFFICE_WALL : ROOM_WALLS[Picked(room.window, room.seed, ROLL_WALL_PAINT, ROOM_WALLS.length())];
}

vec3 RoomFloor(Room room)
{
	return room.offices ? OFFICE_FLOOR : ROOM_FLOORS[Picked(room.window, room.seed, ROLL_FLOOR_PAINT, ROOM_FLOORS.length())];
}

vec3 BlindPaint(Room room)
{
	return BLINDS[room.offices ? room.seed % OFFICE_BLINDS : Picked(room.window, room.seed, ROLL_BLIND_PAINT, BLINDS.length())];
}

vec3 CurtainPaint(Room room)
{
	return CURTAINS[Picked(room.window, room.seed, ROLL_CURTAIN_PAINT, CURTAINS.length())];
}

/* A shop's back wall is shelved, each shelf laid with goods of one colour or another and a few gaps between. */
vec3 Stocked(Room room, vec2 hit, float floor_height)
{
	float shelf = (hit.y - floor_height) / SHELF_PITCH;
	if (fract(shelf) < SHELF_BOARD) return SHELF_PAINT;
	float roll = Hash(ivec2(floor(hit.x / GOODS_PITCH), floor(shelf)) + room.window * GOODS_SALT);
	return roll < GOODS_GAPS ? RoomWall(room) : GOODS[Pick((roll - GOODS_GAPS) / (1.0 - GOODS_GAPS), GOODS.length())];
}

struct Shell {
	vec2 low;
	vec2 high;
	float depth;
};

Shell ShellOf(Room room, Bay bay, uint grid)
{
	float half_width = FACADE_PITCH[grid] * 0.5 - ROOM_PARTITION;
	float floor_height = -bay.opening.sill;
	float ceiling = floor_height + (bay.window.y == 0 ? FACADE_GROUND[grid] : FACADE_STOREY[grid]) - FLOOR_SLAB;
	return Shell(vec2(-half_width, floor_height), vec2(half_width, ceiling), RoomDepth(room));
}

float Placed(float width, Shell shell, float roll)
{
	float free = max(shell.high.x - width * 0.5, 0.0);
	return mix(-free, free, roll);
}

vec3 Fitted(Room room, vec2 hit, Shell shell)
{
	Fitting fitting = BACK_FITTINGS[room.kind];
	vec2 from = vec2(hit.x - Placed(fitting.size.x, shell, Roll(room.window, room.seed, ROLL_FITTING)), hit.y - shell.low.y - fitting.raised);
	bool on_fitting = abs(from.x) < fitting.size.x * 0.5 && from.y >= 0.0 && from.y < fitting.size.y;
	return on_fitting ? fitting.paint : RoomWall(room);
}

vec3 BackWall(Room room, vec2 hit, Shell shell)
{
	return room.kind == ROOM_SHOP ? Stocked(room, hit, shell.low.y) : Fitted(room, hit, shell);
}

Clad PaneOver(vec3 room, float daylight, float sunbeam, vec3 lamp)
{
	return Clad(room, FACING_OUT, PANE_ROUGHNESS, 1.0, 1.0, 1.0, daylight, sunbeam, lamp, 0.0);
}

Opening Unscreened(Opening opening, Room room)
{
	opening.width *= 1.0 - 2.0 * room.drawn;
	opening.height *= 1.0 - room.drop;
	return opening;
}

vec3 LampLightAt(Room room, vec3 point, vec3 lamp)
{
	vec3 from_lamp = point - lamp;
	return room.lamp / (1.0 + dot(from_lamp, from_lamp) / (LAMP_REACH * LAMP_REACH));
}

bool UnderCeilingLamp(Room room, vec2 spot, float half_width, float lamp_depth)
{
	if (room.offices) return abs(spot.x) < half_width * PANEL_SPREAD && abs(fract(spot.y / OFFICE_PANEL_PITCH) - 0.5) * OFFICE_PANEL_PITCH < PANEL_HALF_LENGTH;
	return length(spot - vec2(0.0, lamp_depth)) < HOME_FIXTURE_RADIUS;
}

struct Sightline {
	vec2 from;
	vec2 shift;
	vec2 heading;
};

struct Sight {
	vec3 surface;
	vec2 hit;
	float sunk;
	bool overhead;
};

Sight ShellSeen(Room room, Shell shell, Sightline line)
{
	vec2 reach = ReachOut(line.from, line.heading, shell.low, shell.high);
	float sunk = min(shell.depth, min(reach.x, reach.y));
	vec2 hit = line.from + line.shift * sunk;
	bool on_back = sunk >= shell.depth;
	bool level = !on_back && reach.y < reach.x;
	bool on_ceiling = level && line.heading.y > 0.0;
	vec3 surface = on_back ? BackWall(room, hit, shell) : on_ceiling ? ROOM_CEILING : level ? RoomFloor(room) : RoomWall(room) * ROOM_SIDE_SHADE;
	return Sight(surface, hit, sunk, on_ceiling);
}

Sight PieceSeen(Sight sight, Room room, Shell shell, Sightline line, int slot)
{
	Piece piece = PIECES[int(room.kind) * PIECES_PER_ROOM + slot];
	vec2 rolls = Hash2(Die(room.window, room.seed, ROLL_PIECE + slot));
	float front = max(shell.depth - piece.size.z, 0.0) * mix(piece.set_back.x, piece.set_back.y, rolls.y);
	float width = min(piece.size.x, shell.high.x - shell.low.x);
	float centre = Placed(width, shell, rolls.x);
	vec2 low = vec2(centre - width * 0.5, shell.low.y);
	vec2 high = vec2(centre + width * 0.5, shell.low.y + piece.size.y);
	vec2 enter = ReachIn(line.from, line.heading, low, high);
	vec2 leave = ReachOut(line.from, line.heading, low, high);
	float sunk = max(front, max(enter.x, enter.y));
	if (sunk >= sight.sunk || sunk > min(front + piece.size.z, min(leave.x, leave.y))) return sight;
	vec3 paint = piece.dyed ? CurtainPaint(room) : piece.paint;
	float shade = sunk == front ? 1.0 : sunk == enter.y ? PIECE_TOP_LIGHT : ROOM_SIDE_SHADE;
	return Sight(paint * shade, line.from + line.shift * sunk, sunk, false);
}

Clad RoomSeen(Room room, Bay bay, Elevation elevation)
{
	Shell shell = ShellOf(room, bay, elevation.grid);
	vec2 shift = SightShift(elevation.outlook);
	Sightline line = Sightline(bay.at, shift, Heading(shift));
	Sight sight = ShellSeen(room, shell, line);
	for (int slot = 0; slot < PIECES_PER_ROOM; slot++) sight = PieceSeen(sight, room, shell, line, slot);
	float lamp_depth = shell.depth * LAMP_DEPTH_SHARE;
	bool glowing = sight.overhead && UnderCeilingLamp(room, vec2(sight.hit.x, sight.sunk), shell.high.x, lamp_depth);
	vec3 lamp = glowing ? room.lamp * CEILING_LAMP_GLOW : LampLightAt(room, vec3(sight.hit, sight.sunk), vec3(0.0, shell.high.y, lamp_depth));
	float sunbeam = SUNBEAM_LIGHT * SunThrough(Unscreened(bay.opening, room), sight.hit, sight.sunk, elevation.outlook.sun);
	return PaneOver(sight.surface * room.tone, mix(1.0, ROOM_FAR_LIGHT, sight.sunk / DEEPEST_ROOM), sunbeam, lamp);
}

Clad RoomMean(Room room)
{
	vec3 surfaces = mix(RoomWall(room) * ROOM_SIDE_SHADE, RoomFloor(room), ROOM_MEAN_FLAT_SHARE);
	return PaneOver(surfaces * room.tone, ROOM_MEAN_DAYLIGHT, MEAN_SUNBEAM, room.lamp * ROOM_MEAN_LAMP);
}

Clad Drape(vec3 paint, Room room)
{
	return PaneOver(paint, 1.0, SUNBEAM_LIGHT, room.lamp * DRAPE_GLOW);
}

Clad SeenThrough(Room room, Bay bay, Elevation elevation)
{
	Opening opening = bay.opening;
	if (bay.at.y > opening.height * (1.0 - room.drop)) return Drape(BlindPaint(room), room);
	if (abs(bay.at.x) > opening.width * (0.5 - room.drawn)) return Drape(CurtainPaint(room), room);
	float resolved = 1.0 - smoothstep(ROOM_RESOLVED_PIXELS.x, ROOM_RESOLVED_PIXELS.y, elevation.pixel);
	if (resolved <= 0.0) return RoomMean(room);
	Clad seen = RoomSeen(room, bay, elevation);
	return resolved >= 1.0 ? seen : Mixed(RoomMean(room), seen, resolved);
}

Clad RoomFromAfar(Bay bay, Elevation elevation)
{
	Room room = RoomBehind(bay, elevation);
	Clad seen = RoomMean(room);
	if (room.drawn > 0.0) seen = Mixed(seen, Drape(CurtainPaint(room), room), CURTAIN_SIDES * room.drawn);
	return room.drop > 0.0 ? Mixed(seen, Drape(BlindPaint(room), room), room.drop) : seen;
}

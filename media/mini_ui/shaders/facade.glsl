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
const Opening CURTAIN_PANEL = Opening(OPEN_WINDOW, 7.0, 6.0, 1.0, MUNTIN_NONE, false);
const Opening CURTAIN_LOBBY = Opening(OPEN_WINDOW, 7.0, 10.0, 1.0, MUNTIN_TRANSOM, false);
const Opening CURTAIN_ENTRANCE = Opening(OPEN_GLASS_DOOR, 7.0, 11.0, 0.0, MUNTIN_MULLION, false);
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
const vec3 SILL_STONE = vec3(0.80, 0.78, 0.74);
const float SURROUND_SHARE = 0.55;
const float SURROUND_WIDTH = 1.0;
const float MULLION_WIDTH = 0.4;
const vec3 BLINDS[3] = vec3[3](vec3(0.88, 0.87, 0.84), vec3(0.58, 0.58, 0.56), vec3(0.86, 0.80, 0.66));
const uint OFFICE_BLINDS = 2u;
const vec3 CURTAINS[4] = vec3[4](vec3(0.62, 0.20, 0.16), vec3(0.78, 0.66, 0.42), vec3(0.30, 0.42, 0.30), vec3(0.70, 0.62, 0.58));
const vec3 SHUTTER_SLAT = vec3(0.62, 0.64, 0.66);
const vec3 SHUTTER_GROOVE = vec3(0.50, 0.52, 0.54);
const vec3 DOOR_PAINTS[5] = vec3[5](vec3(0.42, 0.12, 0.10), vec3(0.12, 0.28, 0.18), vec3(0.12, 0.20, 0.36), vec3(0.30, 0.20, 0.12), vec3(0.85, 0.84, 0.80));
const vec3 FASCIA_PAINTS[4] = vec3[4](vec3(0.55, 0.12, 0.10), vec3(0.10, 0.24, 0.40), vec3(0.14, 0.32, 0.18), vec3(0.62, 0.48, 0.16));
const float DRIP_REACH = 5.0;
const float DRIP_SHADE = 0.86;
const float DRIPS_PER_TEXEL = 0.5;
const vec2 DRIP_EDGES = vec2(0.25, 0.85);
const float DRIP_VARIETY = 0.8;
const vec2 DRIP_WINDOW_SEED = vec2(7.0, 13.0);

const ivec2 WINDOW_SALT = ivec2(7, 13);
const int ROLL_STRIDE = 7919;
const int STOREY_COLUMN = -4099;
const int ROLL_TONE = 0;
const int ROLL_LAMP = 1;
const int ROLL_BLIND = 2;
const int ROLL_CURTAIN = 3;
const int ROLL_PAINT = 4;
const int ROLL_WARP = 5;
const vec2 ROOM_TONES = vec2(0.55, 1.35);
const float LIT_ROOM_SHARE = 0.08;
const float LIT_SHOP_SHARE = 0.6;
const float LIT_STOREY_SHARE = 0.18;
const float LIT_IN_LIT_STOREY = 0.75;
const float BLINDED_STOREY_SHARE = 0.3;
const float BLINDED_ROOM_SHARE = 0.22;
const vec2 BLIND_DROPS = vec2(0.15, 0.85);
const float CURTAINED_ROOM_SHARE = 0.35;
const vec2 CURTAIN_REACHES = vec2(0.1, 0.32);
const vec2 PANE_WARP = vec2(0.03, 0.08);
const float PANE_ROUGHNESS = 0.06;
const float GLAZED_SHARE = 0.85;
const vec3 SPANDREL_BACKING = vec3(0.16, 0.17, 0.18);
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

/* How far outside the opening a point lies, in texels; inside is negative. */
float OutsideOpening(Opening opening, vec2 at)
{
	float half_width = opening.width * 0.5;
	float spring = opening.height - half_width;
	if (opening.arch && at.y > spring) return max(length(vec2(at.x, at.y - spring)) - half_width, -at.y);
	return max(abs(at.x) - half_width, max(-at.y, at.y - opening.height));
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
};

float Roll(ivec2 window, uint seed, int roll)
{
	return Hash(window * WINDOW_SALT + ivec2(int(seed), roll * ROLL_STRIDE));
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

vec3 FramePaint(uint seed)
{
	return FRAME_PAINTS[seed % uint(FRAME_PAINTS.length())];
}

Room RoomBehind(Bay bay, uint grid, uint seed)
{
	ivec2 storey = ivec2(STOREY_COLUMN, bay.window.y);
	bool offices = HoldsOffices(grid);
	bool lit_storey = offices && Roll(storey, seed, ROLL_LAMP) < LIT_STOREY_SHARE;
	float lit = bay.opening.kind == OPEN_SHOP ? LIT_SHOP_SHARE : lit_storey ? LIT_IN_LIT_STOREY : LIT_ROOM_SHARE;
	float drop = offices ? Chosen(Roll(storey, seed, ROLL_BLIND), BLINDED_STOREY_SHARE, BLIND_DROPS) : Chosen(Roll(bay.window, seed, ROLL_BLIND), BLINDED_ROOM_SHARE, BLIND_DROPS);
	float drawn = offices ? 0.0 : Chosen(Roll(bay.window, seed, ROLL_CURTAIN), CURTAINED_ROOM_SHARE, CURTAIN_REACHES);
	uint paint = offices ? seed : uint(Roll(bay.window, seed, ROLL_PAINT) * float(BLINDS.length() * CURTAINS.length()));
	return Room(mix(ROOM_TONES.x, ROOM_TONES.y, Roll(bay.window, seed, ROLL_TONE)), Roll(bay.window, seed, ROLL_LAMP) < lit ? 1.0 : 0.0,
		drop, BLINDS[paint % (offices ? OFFICE_BLINDS : uint(BLINDS.length()))], drawn, CURTAINS[paint % uint(CURTAINS.length())]);
}

Clad PaneOver(vec3 room, float lamp)
{
	return Clad(room, vec2(0.0), PANE_ROUGHNESS, 1.0, 1.0, lamp);
}

Clad Glazing(Bay bay, uint grid, uint seed, float reveal)
{
	Room room = RoomBehind(bay, grid, seed);
	Opening opening = bay.opening;
	vec3 seen = DIM_ROOM * room.tone;
	if (bay.at.y > opening.height * (1.0 - room.drop)) seen = room.blind;
	else if (abs(bay.at.x) > opening.width * (0.5 - room.drawn)) seen = room.curtain;
	Clad clad = PaneOver(seen * reveal, room.lamp);
	clad.tilt = (Hash2(bay.window * WINDOW_SALT + ivec2(int(seed), ROLL_WARP * ROLL_STRIDE)) - 0.5) * PANE_WARP;
	return clad;
}

/* What fills an opening: glass behind its bars and frame, a painted door, or the slats of a shutter; the head of every opening lies in the shade of its reveal. */
Clad Filling(Bay bay, uint grid, uint seed, float pixel)
{
	Opening opening = bay.opening;
	vec2 at = bay.at;
	float reveal = mix(1.0, 0.6, smoothstep(opening.height - 1.6, opening.height - 0.4, at.y));
	if (opening.kind == OPEN_DOOR) {
		vec3 paint = DOOR_PAINTS[(seed + uint(bay.window.y)) % 5u];
		return Matte(paint * reveal * (OnMuntin(opening, at, pixel) ? 0.7 : 1.0), 0.6);
	}
	if (opening.kind == OPEN_SHUTTER) {
		bool groove = fract(at.y * 0.5) < 0.5;
		return Matte((at.y > opening.height - 1.0 ? SHUTTER_GROOVE * 0.8 : groove ? SHUTTER_GROOVE : SHUTTER_SLAT) * reveal, 0.45);
	}
	bool rimmed = opening.kind != OPEN_WINDOW && OutsideOpening(opening, at) > -max(0.6, pixel);
	if (rimmed || OnMuntin(opening, at, pixel)) return Matte((rimmed ? vec3(0.22) : FramePaint(seed)) * reveal, 0.5);
	return Glazing(bay, grid, seed, reveal);
}

float Drips(Bay bay, vec2 texel, uint grid)
{
	if (texel.y < FACADE_GROUND[grid] || bay.opening.kind == OPEN_NONE) return 1.0;
	float below = mod(-bay.at.y, FACADE_STOREY[grid]);
	float half_width = bay.opening.width * 0.5;
	float under = 1.0 - smoothstep(half_width - 0.5, half_width + 0.5, abs(bay.at.x));
	float runs = smoothstep(DRIP_EDGES.x, DRIP_EDGES.y, Noise(vec2(bay.at.x * DRIPS_PER_TEXEL, dot(vec2(bay.window), DRIP_WINDOW_SEED))));
	float fade = 1.0 - smoothstep(0.0, DRIP_REACH * Varied(runs, DRIP_VARIETY), below);
	return mix(1.0, DRIP_SHADE, under * runs * fade);
}

Clad FacadeWall(uint material, vec2 pattern, vec3 tint)
{
	return material == CLAD_GLASS ? PaneOver(SPANDREL_BACKING, 0.0) : CladOf(material, pattern, tint);
}

Clad Windowed(Clad wall, uint grid, vec2 texel, bool front, uint seed, float pixel)
{
	Bay bay = BayAt(grid, texel, front);
	Opening opening = bay.opening;
	Clad clad = wall;
	clad.albedo *= Drips(bay, texel, grid);

	float fascia = FASCIA_ROWS[grid];
	float ground = FACADE_GROUND[grid];
	if (front && fascia > 0.0 && texel.y < ground && texel.y >= ground - 1.0 - fascia) {
		clad = Matte(texel.y >= ground - 1.0 ? SILL_STONE : FASCIA_PAINTS[seed % 4u], 0.6);
	}
	if (opening.kind == OPEN_NONE) return clad;

	float outside = OutsideOpening(opening, bay.at);
	bool sills = !Mullioned(grid) && (opening.kind == OPEN_WINDOW || opening.kind == OPEN_SHOP);
	float springline = opening.arch ? opening.height - opening.width * 0.5 : opening.height;
	bool framed = outside < (Mullioned(grid) ? MULLION_WIDTH : SURROUND_WIDTH) && (!sills || bay.at.y >= springline);
	bool on_sill = sills && bay.at.y < 0.0 && bay.at.y >= -1.0 && abs(bay.at.x) < opening.width * 0.5 + 1.0;
	vec3 surround = Mullioned(grid) ? FramePaint(seed) : mix(wall.albedo, SILL_STONE, SURROUND_SHARE);
	if (framed || on_sill) clad = Matte(on_sill ? SILL_STONE : surround, 0.7);
	float inside = clamp(0.5 - outside / max(pixel, 1e-3), 0.0, 1.0);
	return Mixed(clad, Filling(bay, grid, seed, pixel), inside);
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

/* The windows, doors and frames of a facade over the wall it is cut into; seen from too far for them to show, the wall blends toward its windows' glass,
 * the storeys' bands of windows outlasting the windows along them. */
Clad Facade(Clad wall, uint grid, vec2 pattern, bool front, uint seed)
{
	vec2 texel = pattern * TEXELS;
	vec2 pixel = vec2(fwidth(texel.x), fwidth(texel.y));
	float columns = 1.0 - smoothstep(COLUMNS_FADE.x, COLUMNS_FADE.y, max(pixel.x, pixel.y));
	float storeys = 1.0 - smoothstep(STOREYS_FADE.x, STOREYS_FADE.y, pixel.y);
	Opening upper = UPPER_OPENINGS[grid];
	Clad glazed = PaneOver(DIM_ROOM, LIT_ROOM_SHARE);
	Clad far = Mixed(wall, glazed, upper.width * upper.height / (FACADE_PITCH[grid] * FACADE_STOREY[grid]) * GLAZED_SHARE);
	if (storeys <= 0.0) return far;
	Clad banded = Mixed(far, Banded(wall, glazed, grid, texel.y, pixel.y), storeys);
	if (columns <= 0.0) return banded;
	return Mixed(banded, Windowed(wall, grid, texel, front, seed, max(pixel.x, pixel.y)), columns);
}

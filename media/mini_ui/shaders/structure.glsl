const float TAU = 6.2831853;
const float TEXELS = float(TEXELS_PER_TILE);
const float PLINTH_HEIGHT = 0.016;

/* What a point of a building is: its colour before light, how its surface tilts off the face across and up it,
 * how rough and how much glass it is, and how open it lies to the sky. */
struct Clad {
	vec3 albedo;
	vec2 tilt;
	float roughness;
	float glass;
	float occlusion;
};

Clad Matte(vec3 colour, float roughness)
{
	return Clad(colour, vec2(0.0), roughness, 0.0, 1.0);
}

/* How sharply a pattern repeating once a unit of x still shows: whole while a repeat spans a few pixels, gone once it blurs. */
float Sharpness(float x)
{
	return 1.0 - smoothstep(0.2, 0.45, fwidth(x));
}

/* How much of a pixel lines of this width, one a unit, cover; lines too fine to show blend into the share they cover. */
float Lines(float x, float width)
{
	float pixel = fwidth(x);
	float gap = abs(fract(x + 0.5) - 0.5);
	float line = 1.0 - smoothstep(width * 0.5 - pixel * 0.5, width * 0.5 + pixel * 0.5, gap);
	return mix(width, line, Sharpness(x));
}

float Spread(vec2 cell, float variety, float sharpness)
{
	return 1.0 + variety * (Hash(ivec2(floor(cell))) - 0.5) * sharpness;
}

/* Units laid in courses with every other course shifted half a unit, the joints between them filled. */
Clad Coursed(vec2 p, vec3 tint, vec2 units, float joint, float variety, vec3 filling)
{
	float course = p.y * units.y;
	float column = p.x * units.x + 0.5 * floor(course);
	float joints = max(Lines(course, joint), Lines(column, joint * units.x / units.y));
	vec3 unit = tint * Spread(vec2(column, course), variety, Sharpness(course));
	return Matte(mix(unit, filling, joints), 0.85);
}

/* Overlapping courses down a slope, each shaded where the course above overhangs it and lit along its own lower edge. */
Clad Tiled(vec2 p, vec3 tint, vec2 units, float variety, float roughness, float roll)
{
	float course = p.y * units.y;
	float column = p.x * units.x + 0.5 * floor(course);
	float sharp = Sharpness(course);
	float along = fract(course);
	float shade = mix(1.0, mix(0.72, 1.06, smoothstep(0.0, 0.4, along)) * (1.0 - 0.12 * smoothstep(0.88, 1.0, along)), sharp);
	vec3 colour = tint * shade * Spread(vec2(column, course), variety, sharp);
	float gaps = Lines(column, 0.06) * sharp;
	Clad clad = Matte(colour * (1.0 - 0.35 * gaps), roughness);
	clad.tilt = vec2(roll * sin(TAU * column) * Sharpness(column), -0.2 * sharp * (along - 0.5));
	return clad;
}

Clad Mottled(vec2 p, vec3 tint, float frequency, float variety, float roughness)
{
	float noise = mix(0.5, Noise(p * frequency), Sharpness(p.x * frequency));
	return Matte(tint * Varied(noise, variety), roughness);
}

Clad Panelled(vec2 p, vec3 tint, vec2 panels, float joint, float roughness)
{
	float joints = max(Lines(p.x * panels.x, joint), Lines(p.y * panels.y, joint));
	Clad clad = Mottled(p, tint, 9.0, 0.08, roughness);
	clad.albedo *= 1.0 - 0.3 * joints;
	return clad;
}

Clad Ribbed(vec2 p, vec3 tint, float frequency, float depth, float roughness)
{
	float phase = p.x * frequency;
	float sharp = Sharpness(phase);
	Clad clad = Matte(tint * (1.0 + 0.06 * cos(TAU * phase) * sharp), roughness);
	clad.tilt = vec2(depth * sin(TAU * phase) * sharp, 0.0);
	return clad;
}

Clad Boarded(vec2 p, vec3 tint, float boards, float roughness)
{
	float board = p.y * boards;
	float sharp = Sharpness(board);
	float shade = mix(1.0, mix(0.84, 1.06, fract(board)), sharp);
	Clad clad = Matte(tint * shade * Spread(vec2(0.0, board), 0.1, sharp), roughness);
	clad.tilt = vec2(0.0, 0.25 * sharp);
	return clad;
}

Clad Planked(vec2 p, vec3 tint, float planks)
{
	float plank = p.x * planks;
	float sharp = Sharpness(plank);
	vec3 colour = tint * Spread(vec2(plank, 0.0), 0.18, sharp) * (1.0 - 0.4 * Lines(plank, 0.08));
	return Matte(colour, 0.9);
}

/* Frames holding panes; the frames take the tint and the panes the glass. */
Clad Glazed(vec2 p, vec3 frame, vec2 panes, float bar)
{
	float bars = max(Lines(p.x * panes.x, bar), Lines(p.y * panes.y, bar));
	Clad clad = Matte(mix(frame * 0.6, frame, bars), mix(0.08, 0.5, bars));
	clad.glass = 1.0 - bars;
	return clad;
}

/* The members of a braced lattice, as a share of the point they cover. */
float LatticeCover(vec2 p)
{
	vec2 cell = p * 14.0;
	return max(max(Lines(cell.x, 0.16), Lines(cell.y, 0.16)), max(Lines(cell.x + cell.y, 0.12), Lines(cell.x - cell.y, 0.12)));
}

bool IsMasonry(uint material)
{
	return material == CLAD_BRICK || material == CLAD_STONE || material == CLAD_RENDER || material == CLAD_CONCRETE || material == CLAD_TIMBER;
}

/* Masonry walls stand on a darker plinth. */
float Plinth(uint material, vec2 p, float upright)
{
	if (!IsMasonry(material)) return 1.0;
	float pixel = fwidth(p.y);
	return mix(1.0, mix(0.72, 1.0, smoothstep(PLINTH_HEIGHT - pixel, PLINTH_HEIGHT + pixel, p.y)), step(0.7, upright));
}

Clad CladOf(uint material, vec2 p, vec3 tint)
{
	vec3 mortar = mix(tint, vec3(0.78, 0.75, 0.70), 0.7);
	switch (material) {
		case CLAD_BRICK: return Coursed(p, tint, vec2(40.0, 80.0), 0.18, 0.18, mortar);
		case CLAD_STONE: return Coursed(p, tint, vec2(16.0, 32.0), 0.1, 0.12, mortar);
		case CLAD_FOUNDATION: return Coursed(p, tint * 0.9, vec2(12.0, 24.0), 0.1, 0.25, mortar * 0.8);
		case CLAD_RENDER: return Mottled(p, tint, 14.0, 0.07, 0.9);
		case CLAD_CONCRETE: return Panelled(p, tint, vec2(6.0, 8.0), 0.03, 0.85);
		case CLAD_TIMBER: return Boarded(p, tint, 56.0, 0.85);
		case CLAD_PLANKS: return Planked(p, tint, 36.0);
		case CLAD_CORRUGATED: return Ribbed(p, tint, 70.0, 0.35, 0.45);
		case CLAD_METAL: return Panelled(p, tint, vec2(9.0, 0.0), 0.04, 0.4);
		case CLAD_GLASS: return Glazed(p, tint, vec2(12.0, 10.0), 0.08);
		case CLAD_LATTICE: return Matte(tint, 0.5);
		case CLAD_CLAY_TILE: return Tiled(p, tint, vec2(34.0, 42.0), 0.14, 0.65, 0.3);
		case CLAD_SLATE: return Tiled(p, tint, vec2(28.0, 60.0), 0.16, 0.5, 0.0);
		case CLAD_SHINGLE: return Tiled(p, tint, vec2(22.0, 50.0), 0.22, 0.8, 0.0);
		case CLAD_THATCH: return Mottled(vec2(p.x * 8.0, p.y), tint, 9.0, 0.3, 0.95);
		case CLAD_METAL_SEAM: return Ribbed(vec2(p.x * 0.25, p.y), tint, 80.0, 0.12, 0.35);
		case CLAD_GRAVEL: return Mottled(p, tint, 110.0, 0.22, 0.95);
		case CLAD_MEMBRANE: return Panelled(p, tint, vec2(4.0, 0.0), 0.015, 0.8);
		case CLAD_ROOF_DECK: return Planked(p, tint, 30.0);
		case CLAD_GLASS_ROOF: return Glazed(p, vec3(0.7), vec2(14.0, 5.0), 0.1);
		case CLAD_ASPHALT: return Mottled(p, tint, 60.0, 0.12, 0.9);
		default: return Mottled(p, tint, 10.0, 0.05, 0.85);
	}
}

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

const vec3 FRAME_PAINT = vec3(0.86, 0.85, 0.82);
const vec3 SILL_STONE = vec3(0.80, 0.78, 0.74);
const vec3 BLIND = vec3(0.86, 0.80, 0.66);
const vec3 SHUTTER_SLAT = vec3(0.62, 0.64, 0.66);
const vec3 SHUTTER_GROOVE = vec3(0.50, 0.52, 0.54);
const vec3 DOOR_PAINTS[5] = vec3[5](vec3(0.42, 0.12, 0.10), vec3(0.12, 0.28, 0.18), vec3(0.12, 0.20, 0.36), vec3(0.30, 0.20, 0.12), vec3(0.85, 0.84, 0.80));
const vec3 FASCIA_PAINTS[4] = vec3[4](vec3(0.55, 0.12, 0.10), vec3(0.10, 0.24, 0.40), vec3(0.14, 0.32, 0.18), vec3(0.62, 0.48, 0.16));

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

/* What fills an opening: glass behind its bars and frame, a painted door, or the slats of a shutter; the head of every opening lies in the shade of its reveal. */
Clad Filling(Bay bay, vec3 glass, uint seed, float pixel)
{
	Opening opening = bay.opening;
	vec2 at = bay.at;
	float reveal = mix(1.0, 0.6, smoothstep(opening.height - 1.6, opening.height - 0.4, at.y));
	float pane = Hash(ivec2(bay.window) * ivec2(7, 13) + ivec2(int(seed), 0));
	if (opening.kind == OPEN_DOOR) {
		vec3 paint = DOOR_PAINTS[(seed + uint(bay.window.y)) % 5u];
		return Matte(paint * reveal * (OnMuntin(opening, at, pixel) ? 0.7 : 1.0), 0.6);
	}
	if (opening.kind == OPEN_SHUTTER) {
		bool groove = fract(at.y * 0.5) < 0.5;
		return Matte((at.y > opening.height - 1.0 ? SHUTTER_GROOVE * 0.8 : groove ? SHUTTER_GROOVE : SHUTTER_SLAT) * reveal, 0.45);
	}
	bool rimmed = opening.kind != OPEN_WINDOW && OutsideOpening(opening, at) > -max(0.6, pixel);
	if (rimmed || OnMuntin(opening, at, pixel)) return Matte((rimmed ? vec3(0.22) : FRAME_PAINT) * reveal, 0.5);
	if (opening.kind == OPEN_WINDOW && pane < 0.18 && at.y > opening.height * 0.5) return Matte(BLIND * reveal, 0.8);
	Clad clad = Matte(glass * (0.5 + 0.3 * pane) * reveal, 0.06);
	clad.glass = reveal;
	return clad;
}

/* The windows, doors and frames of a facade over the wall it is cut into; seen from too far for them to show, the wall blends toward its windows' glass. */
Clad Facade(Clad wall, uint grid, vec2 pattern, bool front, vec3 glass, uint seed)
{
	vec2 texel = pattern * TEXELS;
	float pixel = max(fwidth(texel.x), fwidth(texel.y));
	Bay bay = BayAt(grid, texel, front);
	Opening opening = bay.opening;
	Clad clad = wall;

	float fascia = FASCIA_ROWS[grid];
	float ground = FACADE_GROUND[grid];
	if (front && fascia > 0.0 && texel.y < ground && texel.y >= ground - 1.0 - fascia) {
		clad = Matte(texel.y >= ground - 1.0 ? SILL_STONE : FASCIA_PAINTS[seed % 4u], 0.6);
	}

	if (opening.kind != OPEN_NONE) {
		float outside = OutsideOpening(opening, bay.at);
		bool sills = opening.kind == OPEN_WINDOW || opening.kind == OPEN_SHOP;
		float springline = opening.arch ? opening.height - opening.width * 0.5 : opening.height;
		bool framed = outside < 1.0 && (!sills || bay.at.y >= springline);
		bool on_sill = sills && bay.at.y < 0.0 && bay.at.y >= -1.0 && abs(bay.at.x) < opening.width * 0.5 + 1.0;
		if (framed || on_sill) clad = Matte(on_sill ? SILL_STONE : mix(wall.albedo, FRAME_PAINT, 0.55), 0.7);
		float inside = clamp(0.5 - outside / max(pixel, 1e-3), 0.0, 1.0);
		Clad filling = Filling(bay, glass, seed, pixel);
		clad = Clad(mix(clad.albedo, filling.albedo, inside), clad.tilt * (1.0 - inside), mix(clad.roughness, filling.roughness, inside), filling.glass * inside, clad.occlusion);
	}

	Opening upper = UPPER_OPENINGS[grid];
	float open_share = upper.width * upper.height / (FACADE_PITCH[grid] * FACADE_STOREY[grid]);
	Clad distant = Clad(mix(wall.albedo, glass * 0.6, open_share), vec2(0.0), mix(wall.roughness, 0.1, open_share), open_share * 0.7, wall.occlusion);
	float detail = 1.0 - smoothstep(1.2, 2.6, pixel);
	return Clad(mix(distant.albedo, clad.albedo, detail), clad.tilt * detail, mix(distant.roughness, clad.roughness, detail), mix(distant.glass, clad.glass, detail), clad.occlusion);
}

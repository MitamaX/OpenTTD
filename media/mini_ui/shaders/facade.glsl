const vec3 FRAME_PAINTS[] = FRAME_TONES;
const vec3 MULLION_PAINTS[] = MULLION_TONES;
const vec3 SILL_STONE = vec3(0.80, 0.78, 0.74);
const float STONE_GRAIN_PER_TEXEL = 1.3;
const float STONE_GRAIN_VARIETY = 0.14;
const vec2 STONE_RESOLVED_PIXELS = vec2(0.4, 1.0);
const float STONE_JOINT_PITCH = 3.0;
const float STONE_JOINT_HALF_WIDTH = 0.08;
const float STONE_JOINT_SHADE = 0.35;
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
const float FASCIA_ROWS[10] = float[10](0.0, 0.0, 0.0, 0.0, 2.0, 0.0, 0.0, 0.0, 0.0, 0.0);
const float LETTERED_HALF_WIDTH = 5.0;
const float DRIP_REACH = 5.0;
const float DRIP_SHADE = 0.86;
const float DRIPS_PER_TEXEL = 0.5;
const vec2 DRIP_EDGES = vec2(0.25, 0.85);
const float DRIP_VARIETY = 0.8;
const vec2 DRIP_WINDOW_SEED = vec2(7.0, 13.0);

const float RECESS_DEPTH = 1.5;
const float CURTAIN_RECESS = 0.3;
const float RECESS_OPENNESS = 0.55;
const float SHADOW_SOFTNESS = 0.3;
const float LEAST_PIXEL = 1e-3;

const vec2 PANE_WARP = vec2(0.015, 0.035);
const float PANE_WARP_SPAN = 2.5;
const vec2 PANE_WARP_SEED = vec2(5.3, 9.1);
const float GLAZED_SHARE = 0.85;
const vec3 LOUVRE_PAINTS[4] = vec3[4](vec3(0.20, 0.42, 0.30), vec3(0.22, 0.38, 0.55), vec3(0.18, 0.50, 0.50), vec3(0.86, 0.84, 0.78));
const float LOUVRE_GROOVE_SHADE = 0.72;
const vec3 TOY_GLASSES[3] = vec3[3](vec3(0.62, 0.86, 0.96), vec3(0.66, 0.94, 0.84), vec3(0.82, 0.76, 0.98));
const vec3 TOY_GLASS = (TOY_GLASSES[0] + TOY_GLASSES[1] + TOY_GLASSES[2]) / 3.0;
const vec3 TOY_LIT_GLASS = vec3(1.0, 0.9, 0.62);
const vec3 TOY_LAMP = vec3(1.0, 0.86, 0.6);
const vec3 TOY_FRAME = vec3(0.95, 0.95, 0.93);
const float TOY_PANE_ROUGHNESS = 0.3;
const float TOY_GLAZING = 0.45;
const float TOY_BULGE = 0.7;

const vec2 COLUMNS_FADE = vec2(1.2, 2.6);
const vec2 STOREYS_FADE = vec2(2.6, 4.5);
const vec2 BAY_GAPS_FADE = vec2(0.25, 0.5);

vec3 FramePaint(uint grid, uint seed)
{
	if (Landscape() == LANDSCAPE_TOYLAND) return TOY_FRAME;
	return Mullioned(grid) ? MULLION_PAINTS[seed % uint(MULLION_PAINTS.length())] : FRAME_PAINTS[seed % uint(FRAME_PAINTS.length())];
}

Clad FrameOf(uint grid, uint seed)
{
	return Matte(FramePaint(grid, seed), FRAME_ROUGHNESS);
}

bool InGroove(float y)
{
	return fract(y * SLATS_PER_TEXEL) < 0.5;
}

Clad ToyPane(vec3 glass, vec3 lamp)
{
	Clad pane = Matte(glass, TOY_PANE_ROUGHNESS);
	pane.glass = TOY_GLAZING;
	pane.lamp = lamp;
	return pane;
}

Clad ToyPaneOf(Bay bay, uint seed)
{
	bool lit = Roll(bay.window, seed, ROLL_LAMP) < LIT_ROOM_SHARE;
	return lit ? ToyPane(TOY_LIT_GLASS, TOY_LAMP) : ToyPane(TOY_GLASSES[Picked(bay.window, seed, ROLL_TONE, TOY_GLASSES.length())], vec3(0.0));
}

/* A candy pane swells out of its frame, flat across its middle and rounding over toward its edges. */
Clad Bulged(Clad pane, Opening opening, vec2 at)
{
	vec3 edge = NearestEdge(opening, at);
	float radius = ToyCornerRadius(vec2(opening.width, opening.height) * 0.5);
	pane.facing = vec3(edge.xy * TOY_BULGE * smoothstep(-radius, 0.0, edge.z), 1.0);
	return pane;
}

Clad MeanGlazing()
{
	return Landscape() == LANDSCAPE_TOYLAND ? ToyPane(TOY_GLASS, TOY_LAMP * LIT_ROOM_SHARE) : PaneOver(DIM_ROOM, 1.0, TYPICAL_LAMP * LampShare(LIT_ROOM_SHARE));
}

/* Neighbouring panes lean alike, the lean turning slowly over a few windows across and up the facade, so what they mirror bends instead of breaking up. */
vec3 PaneLean(ivec2 window, uint seed)
{
	vec2 p = vec2(window) / PANE_WARP_SPAN + float(seed) * PANE_WARP_SEED;
	return vec3((vec2(Noise(p), Noise(p + PANE_WARP_SEED)) - 0.5) * PANE_WARP, 1.0);
}

Clad Glazing(Bay bay, uint grid, uint seed, Outlook outlook, float pixel)
{
	if (Landscape() == LANDSCAPE_TOYLAND) return Bulged(ToyPaneOf(bay, seed), bay.opening, bay.at);
	Clad clad = SeenThrough(RoomBehind(bay, grid, seed), bay, grid, outlook, pixel);
	clad.facing = PaneLean(bay.window, seed);
	return clad;
}

vec3 DoorPaint(Bay bay, uint seed)
{
	return DOOR_PAINTS[(seed + uint(bay.window.y)) % uint(DOOR_PAINTS.length())];
}

/* What fills an opening: glass behind its bars and frame, a painted door, or the slats of a shutter. */
Clad Filling(Bay bay, uint grid, uint seed, Outlook outlook, float pixel)
{
	Opening opening = bay.opening;
	vec2 at = bay.at;
	if (opening.kind == OPEN_DOOR) return Matte(DoorPaint(bay, seed) * (OnMuntin(opening, at, pixel) ? DOOR_PANEL_SHADE : 1.0), PAINT_ROUGHNESS);
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
	if (!Weathers() || texel.y < FACADE_GROUND[grid] || bay.opening.kind == OPEN_NONE) return 1.0;
	float half_width = bay.opening.width * 0.5;
	float under = 1.0 - smoothstep(half_width - 0.5, half_width + 0.5, abs(bay.at.x));
	if (under <= 0.0) return 1.0;
	float below = mod(-bay.at.y, FACADE_STOREY[grid]);
	ivec2 shedding = bay.at.y < 0.0 ? bay.window : bay.window + ivec2(0, 1);
	float runs = smoothstep(DRIP_EDGES.x, DRIP_EDGES.y, Noise(vec2(bay.at.x * DRIPS_PER_TEXEL, dot(vec2(shedding), DRIP_WINDOW_SEED))));
	float fade = 1.0 - smoothstep(0.0, DRIP_REACH * Varied(runs, DRIP_VARIETY), below);
	return mix(1.0, DRIP_SHADE, under * runs * fade);
}

Clad FacadeWall(uint material, vec2 pattern, vec3 tint)
{
	if (material != CLAD_GLASS) return CladOf(material, pattern, tint);
	return Landscape() == LANDSCAPE_TOYLAND ? Matte(tint, TOY_PANE_ROUGHNESS) : PaneOver(SPANDREL_BACKING, 1.0, vec3(0.0));
}

/* Dressed stone about an opening: its own fine grain, and joints across it every few texels along the edge it follows. */
Clad DressedStone(vec2 at, float along, float pixel)
{
	float shown = 1.0 - smoothstep(STONE_RESOLVED_PIXELS.x, STONE_RESOLVED_PIXELS.y, pixel);
	float grain = mix(0.5, Noise(at * STONE_GRAIN_PER_TEXEL), shown);
	float joint = Stroke(abs(fract(along / STONE_JOINT_PITCH + 0.5) - 0.5) * STONE_JOINT_PITCH, STONE_JOINT_HALF_WIDTH, pixel);
	return Matte(SILL_STONE * Varied(grain, STONE_GRAIN_VARIETY) * (1.0 - STONE_JOINT_SHADE * joint), STONE_ROUGHNESS);
}

bool OnFascia(uint grid, vec2 texel, bool front)
{
	float ground = FACADE_GROUND[grid];
	return front && FASCIA_ROWS[grid] > 0.0 && texel.y < ground && texel.y >= ground - 1.0 - FASCIA_ROWS[grid];
}

/* A shopfront's fascia: a painted board lettered with the shop's name across each bay, on a stone string course. */
Clad Fascia(uint grid, vec2 texel, uint seed, float pixel)
{
	float ground = FACADE_GROUND[grid];
	if (texel.y >= ground - 1.0) return DressedStone(texel, texel.x, pixel);
	float pitch = FACADE_PITCH[grid];
	float bay = floor(texel.x / pitch);
	vec2 at = vec2(texel.x - (bay + 0.5) * pitch, texel.y - (ground - 1.0 - FASCIA_ROWS[grid] * 0.5));
	float letters = abs(at.x) < LETTERED_HALF_WIDTH ? Lettering(at, int(bay) + int(seed), pixel) : 0.0;
	return Matte(mix(FASCIA_PAINTS[seed % uint(FASCIA_PAINTS.length())], LETTERING, letters), PAINT_ROUGHNESS);
}

Clad Windowed(Clad wall, uint grid, vec2 texel, bool front, uint seed, Outlook outlook, float pixel)
{
	Bay bay = BayAt(grid, texel, front);
	Opening opening = bay.opening;
	Clad clad = wall;
	clad.albedo *= Drips(bay, texel, grid);
	if (OnFascia(grid, texel, front)) clad = Fascia(grid, texel, seed, pixel);
	if (opening.kind == OPEN_NONE) return clad;

	vec3 edge = NearestEdge(opening, bay.at);
	bool sills = !Mullioned(grid) && (opening.kind == OPEN_WINDOW || opening.kind == OPEN_SHOP);
	float springline = opening.arch ? opening.height - opening.width * 0.5 : opening.height;
	bool surrounded = Mullioned(grid) || Landscape() != LANDSCAPE_TOYLAND;
	bool framed = surrounded && edge.z < (Mullioned(grid) ? MULLION_WIDTH : SURROUND_WIDTH) && (!sills || bay.at.y >= springline);
	bool on_sill = sills && bay.at.y < 0.0 && bay.at.y >= -SILL_HEIGHT && AlongSill(opening, bay.at);
	bool louvred = Landscape() == LANDSCAPE_TROPIC && HoldsHomes(grid) && opening.kind == OPEN_WINDOW && BesideWindow(opening, bay.at);
	if (sills) clad = UnderSill(clad, opening, bay.at, outlook.sun);
	if (on_sill) {
		clad = DressedStone(bay.at, bay.at.x, pixel);
		clad.facing = vec3(0.0, SILL_LEDGE_TILT, 1.0);
	} else if (louvred) {
		clad = Louvre(bay.at, seed);
	} else if (framed) {
		clad = Mullioned(grid) ? FrameOf(grid, seed) : DressedStone(bay.at, dot(bay.at, vec2(-edge.y, edge.x)), pixel);
		clad.facing = vec3(-edge.xy * SURROUND_BEVEL, 1.0);
	}
	float inside = InsideShare(edge.z, pixel);
	if (inside <= 0.0) return clad;
	return Mixed(clad, Recessed(wall, bay, grid, seed, outlook, pixel), inside);
}

Clad FillingFromAfar(Bay bay, uint grid, uint seed)
{
	if (bay.opening.kind == OPEN_DOOR) return Matte(DoorPaint(bay, seed), PAINT_ROUGHNESS);
	if (bay.opening.kind == OPEN_SHUTTER) return Matte(mix(SHUTTER_SLAT, SHUTTER_GROOVE, 0.5), SHUTTER_ROUGHNESS);
	return Landscape() == LANDSCAPE_TOYLAND ? ToyPaneOf(bay, seed) : RoomFromAfar(bay, grid, seed);
}

Clad Banded(Clad wall, uint grid, vec2 texel, bool front, uint seed, vec2 pixel)
{
	Bay bay = BayAt(grid, texel, front);
	Opening opening = bay.opening;
	if (opening.kind == OPEN_NONE) return wall;
	float pitch = FACADE_PITCH[grid];
	float half_width = opening.width * 0.5;
	float gaps_shown = 1.0 - smoothstep(BAY_GAPS_FADE.x, BAY_GAPS_FADE.y, pixel.x / pitch);
	float across = mix(opening.width / pitch, Band(bay.at.x, -half_width, half_width, pixel.x), gaps_shown);
	Clad between = Mullioned(grid) ? FrameOf(grid, seed) : wall;
	Clad storey = Mixed(between, FillingFromAfar(bay, grid, seed), across * GLAZED_SHARE);
	return Mixed(wall, storey, Band(bay.at.y, 0.0, opening.height, pixel.y));
}

/* A facade's windows, doors and frames over its wall, settling from afar into its storeys' bands of glass and then into one glazed wall. */
Clad Facade(Clad wall, uint grid, vec2 pattern, bool front, uint seed, Outlook outlook)
{
	vec2 texel = pattern * TEXELS;
	vec2 pixel = vec2(fwidth(texel.x), fwidth(texel.y));
	float widest = max(pixel.x, pixel.y);
	float columns = 1.0 - smoothstep(COLUMNS_FADE.x, COLUMNS_FADE.y, widest);
	if (columns >= 1.0) return Windowed(wall, grid, texel, front, seed, outlook, widest);
	float storeys = 1.0 - smoothstep(STOREYS_FADE.x, STOREYS_FADE.y, pixel.y);
	Opening upper = UPPER_OPENINGS[grid];
	Clad far = Mixed(wall, MeanGlazing(), upper.width * upper.height / (FACADE_PITCH[grid] * FACADE_STOREY[grid]) * GLAZED_SHARE);
	if (storeys <= 0.0) return far;
	Clad banded = Mixed(far, Banded(wall, grid, texel, front, seed, pixel), storeys);
	if (columns <= 0.0) return banded;
	return Mixed(banded, Windowed(wall, grid, texel, front, seed, outlook, widest), columns);
}

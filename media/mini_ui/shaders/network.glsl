const float FAR_AWAY = 1e4;
const int WINDOW_REACH = 1;
const int WINDOW_SPAN = 2 * WINDOW_REACH + 1;
const ivec2 HOME_TILE = ivec2(0);

const int TRACK_PIECES = 6;
const vec2 TRACK_FROM[TRACK_PIECES] = vec2[TRACK_PIECES](vec2(0.0, 0.5), vec2(0.5, 0.0), vec2(0.0, 0.5), vec2(1.0, 0.5), vec2(0.5, 0.0), vec2(0.0, 0.5));
const vec2 TRACK_TO[TRACK_PIECES] = vec2[TRACK_PIECES](vec2(1.0, 0.5), vec2(0.5, 1.0), vec2(0.5, 0.0), vec2(0.5, 1.0), vec2(1.0, 0.5), vec2(0.5, 1.0));

const int ROAD_ENDS = 4;
const vec2 ROAD_END[ROAD_ENDS] = vec2[ROAD_ENDS](vec2(0.5, 0.0), vec2(1.0, 0.5), vec2(0.5, 1.0), vec2(0.0, 0.5));
const uint ROAD_Y_BITS = 0x5u;
const uint ROAD_X_BITS = 0xAu;

const float BODY_HALF[RAIL_LOOKS] = RAIL_BODY_HALVES;
const float STRIP_HALF[RAIL_LOOKS] = RAIL_STRIP_HALVES;
const float HALO_DEPTH[RAIL_LOOKS] = float[RAIL_LOOKS](0.0, 0.35, 0.25);
const float HALO_WIDTH = 0.07;

const float BED_OVERRUN = 0.05;
const float GRAVEL_FREQUENCY = 13.0;
const float GRAVEL_VARIETY = 0.4;
const float SHOULDER_WIDTH = 0.06;
const float SHOULDER_SHADE = 0.76;
const float CRIB_DEPTH = RAIL_BED_HALF - RAIL_GAUGE_HALF;
const float CRIB_BLEND = 0.03;
const float CRIB_SHADE = 0.88;

const float SLEEPER_HALF_LENGTH = 0.19;
const float SLEEPER_HALF_WIDTH = 0.024;
const vec3 TIMBER = vec3(0.31, 0.25, 0.20);
const float TIMBER_VARIETY = 0.3;

const float RAIL_SIDE_SHADE = 0.6;
const float SHINE_SHARE = 0.45;
const float GROOVE_HALF = 0.008;
const vec3 GROOVE_TONE = vec3(0.09, 0.09, 0.10);

const float CONCRETE_FREQUENCY = 9.0;
const float CONCRETE_VARIETY = 0.12;
const float BEVEL_WIDTH = 0.035;
const float BEVEL_SHADE = 0.72;

const vec3 MARKING_TONE = vec3(0.90, 0.90, 0.86);
const float KERB_WIDTH = 0.035;
const float MARKING_HALF = 0.011;
const float EDGE_LINE_INSET = 0.045;
const float DASHES_PER_TILE = 2.0;
const float DASH_SHARE = 0.5;
const float ASPHALT_FREQUENCY = 17.0;
const float ASPHALT_VARIETY = 0.24;
const float PATCH_FREQUENCY = 1.6;
const float PATCH_VARIETY = 0.12;
const vec2 WHEEL_PATHS = vec2(0.08, 0.22);
const float WHEEL_PATH_HALF = 0.04;
const float WHEEL_WEAR = 0.08;
const float TRAM_BED_SHADE = 0.86;

struct Field {
	float edge;
	float spread;
};

struct Piece {
	vec2 from;
	vec2 to;
	vec2 along;
	vec2 side;
	float span;
};

struct End {
	float past;
	bool open;
	bool overhangs;
};

struct Stretch {
	Field body;
	float within;
};

struct Rails {
	float steel;
	float shine;
	float groove;
};

struct Tracks {
	Field body;
	uint look;
	float sleeper;
	float timber;
	Rails rails;
};

struct Routes {
	Field band;
	Rails rails;
};

struct Street {
	Field asphalt;
	Field bed;
	Rails rails;
	bool kerb;
	uint dashed_road;
};

struct Network {
	vec4 paint;
	vec2 layers;
};

const Field NOWHERE = Field(FAR_AWAY, 1.0);
const Rails NO_RAILS = Rails(0.0, 0.0, 0.0);

ivec2 network_home;
uvec4 network_window[WINDOW_SPAN * WINDOW_SPAN];

uint Bit(int index)
{
	return 1u << uint(index);
}

float TrackDetail()
{
	return Resolved(SLEEPERS_PER_TILE);
}

float RailDetail()
{
	return Resolved(RAIL_FREQUENCY);
}

float MarkingDetail()
{
	return Resolved(MARKING_FREQUENCY);
}

bool PieceDetailShown()
{
	return TrackDetail() > 0.0 || RailDetail() > 0.0;
}

void GatherNetwork(ivec2 home)
{
	network_home = home;
	for (int j = 0; j < WINDOW_SPAN; j++) {
		for (int i = 0; i < WINDOW_SPAN; i++) {
			ivec2 tile = home + ivec2(i, j) - WINDOW_REACH;
			network_window[j * WINDOW_SPAN + i] = OnMap(tile) ? texelFetch(u_network, tile, 0) : uvec4(0u);
		}
	}
}

bool NetworkNear()
{
	uvec3 seen = uvec3(0u);
	for (int slot = 0; slot < WINDOW_SPAN * WINDOW_SPAN; slot++) seen |= network_window[slot].xyz;
	return seen != uvec3(0u);
}

uvec4 NetworkTexel(ivec2 offset)
{
	if (any(greaterThan(abs(offset), ivec2(WINDOW_REACH)))) return uvec4(0u);
	ivec2 slot = offset + WINDOW_REACH;
	return network_window[slot.y * WINDOW_SPAN + slot.x];
}

float Inside(Field field)
{
	return clamp(0.5 - field.edge / field.spread, 0.0, 1.0);
}

Field Grown(Field field, float by)
{
	return Field(field.edge - by, field.spread);
}

Field Nearer(Field a, Field b)
{
	return a.edge <= b.edge ? a : b;
}

Field Farther(Field a, Field b)
{
	return a.edge >= b.edge ? a : b;
}

Field SegmentField(vec2 at, vec2 from, vec2 to, mat2 pixel)
{
	vec2 run = normalize(to - from);
	return Field(SegmentDistance(at, from, to), Spread(pixel, vec2(-run.y, run.x)));
}

float OutsideTile(vec2 local)
{
	return length(max(abs(local - TILE_CENTRE) - HALF_TILE, 0.0));
}

Rails Merged(Rails a, Rails b)
{
	return Rails(max(a.steel, b.steel), max(a.shine, b.shine), max(a.groove, b.groove));
}

/* The offset runs from the rail's centre line away from the track's middle, so the groove lies on the inner side. */
void AddRail(inout Rails rails, float offset, float width, float spread, float mask)
{
	rails.steel = max(rails.steel, Stroke(abs(offset), width, spread) * mask);
	rails.shine = max(rails.shine, Stroke(abs(offset), width * SHINE_SHARE, spread) * mask);
	rails.groove = max(rails.groove, Stroke(abs(offset + width + GROOVE_HALF), GROOVE_HALF, spread) * mask);
}

Piece TrackPiece(ivec2 tile, int index)
{
	vec2 from = vec2(tile) + TRACK_FROM[index];
	vec2 to = vec2(tile) + TRACK_TO[index];
	float span = distance(from, to);
	vec2 along = (to - from) / span;
	return Piece(from, to, along, vec2(-along.y, along.x), span);
}

float LevelOn(ivec2 tile, vec2 at)
{
	return FacetLevel(SurfaceOf(network_home + tile), at - vec2(tile));
}

/* Where two tiles' surfaces stand apart at the joint, an open end stops at the seam and nothing of it lands on the other surface. */
bool Stepped(vec2 joint, vec2 arrival, ivec2 beyond)
{
	ivec2 here = ivec2(floor(joint - arrival * HALF_TILE));
	return LevelOn(here, joint) != LevelOn(beyond, joint);
}

/* A piece's strip ends on the bisector with every piece that continues it across the seam, so their rails meet in a mitre and split the joint without overlap. */
End EndOf(vec2 at, vec2 joint, vec2 arrival)
{
	ivec2 beyond = ivec2(floor(joint + arrival * HALF_TILE));
	uint partners = NetworkTexel(beyond).x;
	vec2 offset = at - joint;
	End end = End(dot(offset, arrival), true, false);
	for (int index = 0; index < TRACK_PIECES; index++) {
		if ((partners & Bit(index)) == 0u) continue;
		Piece next = TrackPiece(beyond, index);
		if (next.from != joint && next.to != joint) continue;
		float past = dot(offset, normalize(arrival + (next.from == joint ? next.along : -next.along)));
		end.past = end.open ? past : min(end.past, past);
		end.open = false;
	}
	end.overhangs = end.open && beyond == HOME_TILE && Stepped(joint, arrival, beyond);
	return end;
}

/* Sleepers are centred on equal slots along the piece, so consecutive pieces keep the pitch across the seam. */
void AddSleeper(inout Tracks tracks, Piece piece, vec2 at, ivec2 seed, mat2 pixel, float within)
{
	vec2 offset = at - piece.from;
	float count = max(round(piece.span * SLEEPERS_PER_TILE), 1.0);
	float pitch = piece.span / count;
	float along = dot(offset, piece.along);
	float slot = clamp(floor(along / pitch), 0.0, count - 1.0);
	float width_cover = Stroke(abs(along - (slot + 0.5) * pitch), SLEEPER_HALF_WIDTH, Spread(pixel, piece.along));
	float length_cover = Inside(Field(abs(dot(offset, piece.side)) - SLEEPER_HALF_LENGTH, Spread(pixel, piece.side)));
	float sleeper = width_cover * length_cover * within;
	if (sleeper <= tracks.sleeper) return;

	tracks.sleeper = sleeper;
	tracks.timber = Varied(Hash(ivec2(seed.x, seed.y * int(SLEEPERS_PER_TILE) + int(slot))), TIMBER_VARIETY);
}

Stretch PieceStretch(Piece piece, Field body, vec2 at, mat2 pixel)
{
	End head = EndOf(at, piece.to, piece.along);
	End tail = EndOf(at, piece.from, -piece.along);
	if (head.overhangs || tail.overhangs) return Stretch(NOWHERE, 0.0);
	float lengthwise = Spread(pixel, piece.along);
	if (head.open) body = Farther(body, Field(head.past - BED_OVERRUN, lengthwise));
	if (tail.open) body = Farther(body, Field(tail.past - BED_OVERRUN, lengthwise));
	return Stretch(body, step(max(head.past, tail.past), 0.0));
}

void AddBody(inout Tracks tracks, Field body, uint look)
{
	if (body.edge >= tracks.body.edge) return;
	tracks.body = body;
	tracks.look = look;
}

void AddPieceDetail(inout Tracks tracks, Piece piece, vec2 at, uint look, float within, ivec2 seed, mat2 pixel)
{
	float side = dot(at - piece.from, piece.side);
	if (look != LOOK_MONORAIL) AddRail(tracks.rails, abs(side) - RAIL_GAUGE_HALF, STRIP_HALF[int(look)], Spread(pixel, piece.side), within);
	if (look == LOOK_RAIL) AddSleeper(tracks, piece, at, seed, pixel, within);
}

void AddPiece(inout Tracks tracks, vec2 at, ivec2 tile, int index, uint look, ivec2 home, mat2 pixel)
{
	Piece piece = TrackPiece(tile, index);
	Field body = Grown(SegmentField(at, piece.from, piece.to, pixel), mix(DISTANT_RAIL_HALF, BODY_HALF[int(look)], TrackDetail()));
	if (body.edge > body.spread + HALO_WIDTH) return;
	if (!PieceDetailShown()) {
		AddBody(tracks, body, look);
		return;
	}

	Stretch stretch = PieceStretch(piece, body, at, pixel);
	AddBody(tracks, stretch.body, look);
	ivec2 world = home + tile;
	AddPieceDetail(tracks, piece, at, look, stretch.within, ivec2(world.x * TRACK_PIECES + index, world.y), pixel);
}

/* Pieces of the neighbouring tiles reach over the seam, so the bed and the rails are unions over the whole window. */
Tracks TracksNear(vec2 at, ivec2 home, mat2 pixel)
{
	Tracks tracks = Tracks(NOWHERE, LOOK_RAIL, 0.0, 1.0, NO_RAILS);
	float reach = RAIL_BED_HALF + HALO_WIDTH + max(Spread(pixel, vec2(1.0, 0.0)), Spread(pixel, vec2(0.0, 1.0)));
	for (int j = -WINDOW_REACH; j <= WINDOW_REACH; j++) {
		for (int i = -WINDOW_REACH; i <= WINDOW_REACH; i++) {
			ivec2 tile = ivec2(i, j);
			uvec4 texel = NetworkTexel(tile);
			if (texel.x == 0u || OutsideTile(at - vec2(tile)) > reach) continue;
			uint look = texel.w & RAIL_LOOK_MASK;
			for (int index = 0; index < TRACK_PIECES; index++) {
				if ((texel.x & Bit(index)) != 0u) AddPiece(tracks, at, tile, index, look, home, pixel);
			}
		}
	}
	return tracks;
}

bool Opposite(int from, int to)
{
	return to - from == ROAD_ENDS / 2;
}

Field RouteCentre(vec2 at, int from, int to, mat2 pixel)
{
	vec2 a = ROAD_END[from];
	vec2 b = ROAD_END[to];
	if (from == to) return SegmentField(at, TILE_CENTRE, a, pixel);
	if (Opposite(from, to)) return SegmentField(at, a, b, pixel);

	vec2 radial = at - (a + b - TILE_CENTRE);
	float reach = max(length(radial), MIN_SPREAD);
	return Field(abs(reach - HALF_TILE), Spread(pixel, radial / reach));
}

/* Every pair of a tile's ends is a route through it, bending around the corner they share; a lone end is a dead end at the centre. */
Routes RoutesThrough(vec2 at, uint ends, float width, mat2 pixel)
{
	Routes routes = Routes(NOWHERE, NO_RAILS);
	bool lone = (ends & (ends - 1u)) == 0u;
	for (int from = 0; from < ROAD_ENDS; from++) {
		for (int to = from; to < ROAD_ENDS; to++) {
			if ((ends & Bit(from)) == 0u || (ends & Bit(to)) == 0u || (from == to && !lone)) continue;
			Field centre = RouteCentre(at, from, to, pixel);
			routes.band = Nearer(routes.band, Grown(centre, width));
			AddRail(routes.rails, centre.edge - RAIL_GAUGE_HALF, RAIL_HALF, centre.spread, 1.0);
		}
	}
	return routes;
}

Street StreetAt(vec2 at, uvec4 here, mat2 pixel)
{
	Routes road = RoutesThrough(at, here.y, ROAD_HALF, pixel);
	Routes tram = RoutesThrough(at, here.z, TRAM_BED_HALF, pixel);
	bool plain = here.x == 0u && here.z == 0u && (here.y == ROAD_X_BITS || here.y == ROAD_Y_BITS);
	return Street(road.band, here.y == 0u ? tram.band : NOWHERE, tram.rails, (here.w & KERB_BIT) != 0u, plain ? here.y : 0u);
}

float CentreDashes(vec2 at, uint dashed_road, mat2 pixel)
{
	if (dashed_road == 0u) return 0.0;
	vec2 run = dashed_road == ROAD_X_BITS ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
	float phase = fract(dot(at, run) * DASHES_PER_TILE);
	float gap = (abs(phase - 0.5) - 0.5 * DASH_SHARE) / DASHES_PER_TILE;
	float stripe = Stroke(abs(dot(at - TILE_CENTRE, run.yx)), MARKING_HALF, Spread(pixel, run.yx));
	return stripe * Inside(Field(gap, Spread(pixel, run)));
}

float Markings(Street street, vec2 at, mat2 pixel)
{
	float edges = Stroke(abs(street.asphalt.edge + EDGE_LINE_INSET), MARKING_HALF, street.asphalt.spread);
	return max(edges, CentreDashes(at, street.dashed_road, pixel));
}

vec3 Ballast(float depth, vec2 p)
{
	float shoulder = mix(SHOULDER_SHADE, 1.0, smoothstep(0.0, SHOULDER_WIDTH, depth));
	float crib = mix(1.0, CRIB_SHADE, smoothstep(CRIB_DEPTH - CRIB_BLEND, CRIB_DEPTH + CRIB_BLEND, depth));
	return BALLAST * Varied(Layered(p, GRAVEL_FREQUENCY), GRAVEL_VARIETY) * shoulder * crib;
}

vec3 Concrete(float depth, vec2 p)
{
	float bevel = mix(BEVEL_SHADE, 1.0, smoothstep(0.0, BEVEL_WIDTH, depth));
	return CONCRETE * bevel * Varied(Octave(p, CONCRETE_FREQUENCY), CONCRETE_VARIETY);
}

vec3 Asphalt(float depth, vec2 p)
{
	vec2 rut = abs(depth - WHEEL_PATHS);
	float wear = 1.0 - smoothstep(0.0, WHEEL_PATH_HALF, min(rut.x, rut.y));
	return ASPHALT * Varied(Octave(p, ASPHALT_FREQUENCY), ASPHALT_VARIETY) * Varied(Layered(p, PATCH_FREQUENCY), PATCH_VARIETY) * (1.0 - WHEEL_WEAR * wear);
}

vec3 BedTone(Tracks tracks, vec2 p)
{
	return tracks.look == LOOK_RAIL ? Ballast(-tracks.body.edge, p) : Concrete(-tracks.body.edge, p);
}

bool Textured(Field field, float detail)
{
	return detail > 0.0 && Inside(field) > 0.0;
}

vec4 TrackBed(Tracks tracks, vec2 p)
{
	float detail = TrackDetail();
	vec3 body = Textured(tracks.body, detail) ? mix(DISTANT_RAIL, BedTone(tracks, p), detail) : DISTANT_RAIL;
	float halo = HALO_DEPTH[int(tracks.look)] * (1.0 - smoothstep(0.0, HALO_WIDTH, tracks.body.edge));
	vec4 paint = Over(vec4(0.0), vec3(0.0), halo);
	paint = Over(paint, body, Inside(tracks.body));
	return Over(paint, TIMBER * tracks.timber, tracks.sleeper * detail);
}

vec4 Paved(vec4 paint, Street street, vec2 at, vec2 p, mat2 pixel)
{
	float detail = MarkingDetail();
	vec3 bed = Textured(street.bed, detail) ? mix(DISTANT_ROAD, Concrete(-street.bed.edge, p) * TRAM_BED_SHADE, detail) : DISTANT_ROAD;
	vec3 asphalt = Textured(street.asphalt, detail) ? mix(DISTANT_ROAD, Asphalt(-street.asphalt.edge, p), detail) : DISTANT_ROAD;
	if (street.kerb) paint = Over(paint, CONCRETE, Inside(Grown(street.asphalt, KERB_WIDTH)) * detail);
	paint = Over(paint, bed, Inside(street.bed));
	paint = Over(paint, asphalt, Inside(street.asphalt));
	return Over(paint, MARKING_TONE, Markings(street, at, pixel) * Inside(street.asphalt) * detail);
}

vec4 Railed(vec4 paint, Rails rails, float embedded)
{
	float detail = RailDetail();
	paint = Over(paint, GROOVE_TONE, rails.groove * embedded * detail);
	paint = Over(paint, STEEL * RAIL_SIDE_SHADE, rails.steel * detail);
	return Over(paint, STEEL, rails.shine * detail);
}

Network NetworkAt(vec2 p, mat2 pixel)
{
	ivec2 home = ivec2(floor(p));
	GatherNetwork(home);
	if (!NetworkNear()) return Network(vec4(0.0), vec2(0.0));

	vec2 at = p - vec2(home);
	Tracks tracks = TracksNear(at, home, pixel);
	Street street = StreetAt(at, NetworkTexel(HOME_TILE), pixel);
	float paved = max(Inside(street.asphalt), Inside(street.bed));

	vec4 paint = TrackBed(tracks, p);
	paint = Paved(paint, street, at, p, pixel);
	paint = Railed(paint, Merged(tracks.rails, street.rails), paved);
	return Network(paint, vec2(Inside(tracks.body), paved));
}

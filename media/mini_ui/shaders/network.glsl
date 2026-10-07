const float FAR_AWAY = 1e4;
const int WINDOW_REACH = 1;

const int TRACK_PIECES = 6;
const vec2 TRACK_FROM[TRACK_PIECES] = vec2[TRACK_PIECES](vec2(0.0, 0.5), vec2(0.5, 0.0), vec2(0.0, 0.5), vec2(1.0, 0.5), vec2(0.5, 0.0), vec2(0.0, 0.5));
const vec2 TRACK_TO[TRACK_PIECES] = vec2[TRACK_PIECES](vec2(1.0, 0.5), vec2(0.5, 1.0), vec2(0.5, 0.0), vec2(0.5, 1.0), vec2(1.0, 0.5), vec2(0.5, 1.0));

const float THIN_WAY_HALF_PIXELS = 0.55;
const float THIN_WAY_PPT = 2.5;
const float FAR_WAY_OPACITY = 0.35;
const float FAR_STREET_SHARE = 0.3;

const int ROAD_ENDS = 4;
const vec2 ROAD_END[ROAD_ENDS] = vec2[ROAD_ENDS](vec2(0.5, 0.0), vec2(1.0, 0.5), vec2(0.5, 1.0), vec2(0.0, 0.5));

struct Field {
	float edge;
	float spread;
};

struct Network {
	vec4 paint;
	vec2 layers;
};

const Field NOWHERE = Field(FAR_AWAY, 1.0);

uint Bit(int index)
{
	return 1u << uint(index);
}

uvec4 NetworkTexel(ivec2 tile)
{
	return OnMap(tile) ? texelFetch(u_network, tile, 0) : uvec4(0u);
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

Field SegmentField(vec2 at, vec2 from, vec2 to, mat2 pixel)
{
	vec2 run = normalize(to - from);
	return Field(SegmentDistance(at, from, to), Spread(pixel, vec2(-run.y, run.x)));
}

float OutsideTile(vec2 local)
{
	return length(max(abs(local - TILE_CENTRE) - HALF_TILE, 0.0));
}

/* How far toward a thin faint line the far network has drawn in, so from afar it reads as fine lines over the land rather than a dark web. */
float Thinned()
{
	return 1.0 - smoothstep(THIN_WAY_PPT, NETWORK_OPAQUE_PPT, tile_pixels);
}

float WayHalf(float half_width)
{
	return mix(half_width, min(half_width, THIN_WAY_HALF_PIXELS / tile_pixels), Thinned());
}

const float UNBENT = 128.0;

/* How far the eased line slides along a tile's west and north sides. */
vec2 SidesSlid(ivec2 tile)
{
	return OnMap(tile) ? (vec2(texelFetch(u_bends, tile, 0).xy) - UNBENT) / BEND_STEPS_PER_TILE : vec2(0.0);
}

/* Where a piece's end lies on its tile's side once the eased line has slid along it, read from the tile whose west or north side it is. */
vec2 Slid(ivec2 tile, vec2 end)
{
	if (end.x == 0.0) return end + vec2(0.0, SidesSlid(tile).x);
	if (end.x == 1.0) return end + vec2(0.0, SidesSlid(tile + ivec2(1, 0)).x);
	if (end.y == 0.0) return end + vec2(SidesSlid(tile).y, 0.0);
	return end + vec2(SidesSlid(tile + ivec2(0, 1)).y, 0.0);
}

/* Pieces of the neighbouring tiles reach over the seam, so the band is a union over the tiles around that come near enough to reach.
 * Each piece runs straight between its ends where the eased line crosses the tile's sides. */
Field TracksNear(vec2 at, ivec2 home, mat2 pixel)
{
	Field band = NOWHERE;
	float half_width = WayHalf(DISTANT_RAIL_HALF);
	float reach = half_width + max(Spread(pixel, vec2(1.0, 0.0)), Spread(pixel, vec2(0.0, 1.0)));
	for (int j = -WINDOW_REACH; j <= WINDOW_REACH; j++) {
		for (int i = -WINDOW_REACH; i <= WINDOW_REACH; i++) {
			ivec2 tile = ivec2(i, j);
			if (OutsideTile(at - vec2(tile)) > reach) continue;
			uint pieces = NetworkTexel(home + tile).x;
			if (pieces == 0u) continue;
			for (int index = 0; index < TRACK_PIECES; index++) {
				if ((pieces & Bit(index)) == 0u) continue;
				vec2 offset = vec2(tile);
				ivec2 owner = home + tile;
				band = Nearer(band, Grown(SegmentField(at, offset + Slid(owner, TRACK_FROM[index]), offset + Slid(owner, TRACK_TO[index]), pixel), half_width));
			}
		}
	}
	return band;
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
Field RoutesThrough(vec2 at, uint ends, float width, mat2 pixel)
{
	Field band = NOWHERE;
	bool lone = (ends & (ends - 1u)) == 0u;
	for (int from = 0; from < ROAD_ENDS; from++) {
		for (int to = from; to < ROAD_ENDS; to++) {
			if ((ends & Bit(from)) == 0u || (ends & Bit(to)) == 0u || (from == to && !lone)) continue;
			band = Nearer(band, Grown(RouteCentre(at, from, to, pixel), width));
		}
	}
	return band;
}

/* Far off, where the network's meshes have faded out, each way shows as a flat band of its colour, and town streets all but vanish into the blocks they serve. */
Network NetworkAt(vec2 p, mat2 pixel)
{
	ivec2 home = ivec2(floor(p));
	vec2 at = p - vec2(home);
	uvec4 here = NetworkTexel(home);
	float rail = Inside(TracksNear(at, home, pixel));
	bool roads = (here.y | here.z) != 0u;
	if (rail <= 0.0 && !roads) return Network(vec4(0.0), vec2(0.0));
	float road = roads ? Inside(Nearer(RoutesThrough(at, here.y, WayHalf(ROAD_HALF), pixel), RoutesThrough(at, here.z, WayHalf(TRAM_BED_HALF), pixel))) : 0.0;
	float opacity = mix(1.0, FAR_WAY_OPACITY, Thinned());
	float street = mix(1.0, FAR_STREET_SHARE, (here.w & NETWORK_KERB_BIT) != 0u ? Thinned() : 0.0);
	vec4 paint = Over(vec4(0.0), DISTANT_RAIL, rail * opacity);
	paint = Over(paint, DISTANT_ROAD, road * opacity * street);
	return Network(paint, vec2(rail, road));
}

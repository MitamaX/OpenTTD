const float FAR_AWAY = 1e4;
const int WINDOW_REACH = 1;
const int WINDOW_SPAN = 2 * WINDOW_REACH + 1;
const ivec2 HOME_TILE = ivec2(0);

const int TRACK_PIECES = 6;
const vec2 TRACK_FROM[TRACK_PIECES] = vec2[TRACK_PIECES](vec2(0.0, 0.5), vec2(0.5, 0.0), vec2(0.0, 0.5), vec2(1.0, 0.5), vec2(0.5, 0.0), vec2(0.0, 0.5));
const vec2 TRACK_TO[TRACK_PIECES] = vec2[TRACK_PIECES](vec2(1.0, 0.5), vec2(0.5, 1.0), vec2(0.5, 0.0), vec2(0.5, 1.0), vec2(1.0, 0.5), vec2(0.5, 1.0));

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

uvec4 network_window[WINDOW_SPAN * WINDOW_SPAN];

uint Bit(int index)
{
	return 1u << uint(index);
}

void GatherNetwork(ivec2 home)
{
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

Field SegmentField(vec2 at, vec2 from, vec2 to, mat2 pixel)
{
	vec2 run = normalize(to - from);
	return Field(SegmentDistance(at, from, to), Spread(pixel, vec2(-run.y, run.x)));
}

float OutsideTile(vec2 local)
{
	return length(max(abs(local - TILE_CENTRE) - HALF_TILE, 0.0));
}

/* Pieces of the neighbouring tiles reach over the seam, so the band is a union over the whole window. */
Field TracksNear(vec2 at, mat2 pixel)
{
	Field band = NOWHERE;
	float reach = DISTANT_RAIL_HALF + max(Spread(pixel, vec2(1.0, 0.0)), Spread(pixel, vec2(0.0, 1.0)));
	for (int j = -WINDOW_REACH; j <= WINDOW_REACH; j++) {
		for (int i = -WINDOW_REACH; i <= WINDOW_REACH; i++) {
			ivec2 tile = ivec2(i, j);
			uint pieces = NetworkTexel(tile).x;
			if (pieces == 0u || OutsideTile(at - vec2(tile)) > reach) continue;
			for (int index = 0; index < TRACK_PIECES; index++) {
				if ((pieces & Bit(index)) == 0u) continue;
				vec2 offset = vec2(tile);
				band = Nearer(band, Grown(SegmentField(at, offset + TRACK_FROM[index], offset + TRACK_TO[index], pixel), DISTANT_RAIL_HALF));
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

/* Far off, where the network's meshes have faded out, each way shows as a flat band of its colour. */
Network NetworkAt(vec2 p, mat2 pixel)
{
	ivec2 home = ivec2(floor(p));
	GatherNetwork(home);
	if (!NetworkNear()) return Network(vec4(0.0), vec2(0.0));

	vec2 at = p - vec2(home);
	uvec4 here = NetworkTexel(HOME_TILE);
	float rail = Inside(TracksNear(at, pixel));
	float road = Inside(Nearer(RoutesThrough(at, here.y, ROAD_HALF, pixel), RoutesThrough(at, here.z, TRAM_BED_HALF, pixel)));
	vec4 paint = Over(vec4(0.0), DISTANT_RAIL, rail);
	paint = Over(paint, DISTANT_ROAD, road);
	return Network(paint, vec2(rail, road));
}

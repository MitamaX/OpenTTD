uniform usampler2D u_surfaces;
uniform usampler2D u_tiles;
uniform sampler2D u_water;
uniform usampler2D u_network;

const float HALF_TILE = 0.5;
const vec2 TILE_CENTRE = vec2(HALF_TILE);
const float MIN_SQUARED_SPAN = 1e-6;
const float MIN_SPREAD = 1e-4;

const vec2 WEST_CORNER = vec2(1.0, 0.0);

const int NEIGHBOURHOOD_REACH = 1;
const int NEIGHBOURHOOD_SPAN = 2 * NEIGHBOURHOOD_REACH + 1;

struct Corners {
	float north;
	float west;
	float east;
	float south;
};

ivec2 near_origin;
uvec4 near_codes[NEIGHBOURHOOD_SPAN * NEIGHBOURHOOD_SPAN];

/* How many screen pixels one tile spans at the fragment, which every detail fades in by. */
float tile_pixels;

uvec2 Pcg(uvec2 v)
{
	v = v * 1664525u + 1013904223u;
	v.x += v.y * 1664525u;
	v.y += v.x * 1664525u;
	v ^= v >> 16u;
	v.x += v.y * 1664525u;
	v.y += v.x * 1664525u;
	v ^= v >> 16u;
	return v;
}

vec2 Hash2(ivec2 cell)
{
	return vec2(Pcg(uvec2(cell))) * (1.0 / 4294967295.0);
}

float Hash(ivec2 cell)
{
	return Hash2(cell).x;
}

float Noise(vec2 p)
{
	ivec2 cell = ivec2(floor(p));
	vec2 f = fract(p);
	vec2 s = f * f * (3.0 - 2.0 * f);
	float a = Hash(cell);
	float b = Hash(cell + ivec2(1, 0));
	float c = Hash(cell + ivec2(0, 1));
	float d = Hash(cell + ivec2(1, 1));
	return mix(mix(a, b, s.x), mix(c, d, s.x), s.y);
}

/* The way from the nearest of the points scattered one to a cell over to p. */
vec2 CellOffset(vec2 p)
{
	ivec2 cell = ivec2(floor(p));
	vec2 f = fract(p);
	vec2 nearest = vec2(8.0);
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			ivec2 offset = ivec2(x, y);
			vec2 d = vec2(offset) + Hash2(cell + offset) - f;
			if (dot(d, d) < dot(nearest, nearest)) nearest = d;
		}
	}
	return -nearest;
}

float Cells(vec2 p)
{
	return length(CellOffset(p));
}

float Resolved(float frequency)
{
	return smoothstep(UNRESOLVED_REPEAT_PIXELS, RESOLVED_REPEAT_PIXELS, tile_pixels / frequency);
}

float Octave(vec2 p, float frequency)
{
	return mix(0.5, Noise(p * frequency), Resolved(frequency));
}

float Layered(vec2 p, float frequency)
{
	return (2.0 * Octave(p, frequency) + Octave(p + 17.3, frequency * 2.07)) / 3.0;
}

float Varied(float noise, float variety)
{
	return 1.0 + variety * (noise - 0.5);
}

ivec2 Clamped(ivec2 tile)
{
	return clamp(tile, ivec2(0), ivec2(MapSize()) - 1);
}

bool OnMap(ivec2 tile)
{
	return all(greaterThanEqual(tile, ivec2(0))) && all(lessThan(tile, ivec2(MapSize())));
}

Corners SurfaceOf(ivec2 tile)
{
	vec4 levels = vec4(texelFetch(u_surfaces, Clamped(tile), 0));
	return Corners(levels.r, levels.g, levels.b, levels.a);
}

/* The game folds a tile along the diagonal whose corners stand level, and along the lower pair when both do. */
bool FoldsWestToEast(Corners c)
{
	return c.west == c.east && (c.north != c.south || c.north > c.west);
}

/* A tile's ground is two planes meeting on the diagonal the game's GetPartialPixelZ splits it along. */
float FacetLevel(Corners c, vec2 f)
{
	vec2 north_edges = vec2(c.west - c.north, c.east - c.north);
	vec2 south_edges = vec2(c.south - c.east, c.south - c.west);
	if (FoldsWestToEast(c)) {
		vec2 slope = f.x + f.y <= 1.0 ? north_edges : south_edges;
		return c.west + dot(slope, f - WEST_CORNER);
	}
	vec2 slope = f.x >= f.y ? vec2(north_edges.x, south_edges.y) : vec2(south_edges.x, north_edges.y);
	return c.north + dot(slope, f);
}

void GatherNeighbourhood(ivec2 tile)
{
	near_origin = tile - NEIGHBOURHOOD_REACH;
	for (int j = 0; j < NEIGHBOURHOOD_SPAN; j++) {
		for (int i = 0; i < NEIGHBOURHOOD_SPAN; i++) near_codes[j * NEIGHBOURHOOD_SPAN + i] = texelFetch(u_tiles, Clamped(near_origin + ivec2(i, j)), 0);
	}
}

uvec4 CodesAt(ivec2 tile)
{
	ivec2 slot = clamp(tile - near_origin, ivec2(0), ivec2(NEIGHBOURHOOD_SPAN - 1));
	return near_codes[slot.y * NEIGHBOURHOOD_SPAN + slot.x];
}

vec2 SegmentOffset(vec2 p, vec2 from, vec2 to)
{
	vec2 span = to - from;
	float t = clamp(dot(p - from, span) / max(dot(span, span), MIN_SQUARED_SPAN), 0.0, 1.0);
	return p - (from + span * t);
}

float SegmentDistance(vec2 p, vec2 from, vec2 to)
{
	return length(SegmentOffset(p, from, to));
}

/* How wide one screen pixel is on the ground across an edge with this normal; the pixel's columns are the ground it steps over across and down the screen. */
float Spread(mat2 pixel, vec2 normal)
{
	return max(abs(dot(pixel[0], normal)) + abs(dot(pixel[1], normal)), MIN_SPREAD);
}

/* How far apart on the ground two pixels lie square across an edge with this normal as it shows on screen. */
float Pitch(mat2 pixel, vec2 normal)
{
	return max(length(normal * pixel), MIN_SPREAD);
}

/* A line thinner than the pixel keeps the pixel's width and fades by what it lacks, so it thins out instead of breaking up. */
float Stroke(float gap, float width, float pixel)
{
	float drawn = max(width, 0.5 * pixel);
	return clamp(0.5 + (drawn - gap) / pixel, 0.0, 1.0) * width / drawn;
}

float PixelLine(float gap, float pixel, float half_pixels)
{
	return Stroke(gap, half_pixels * pixel, pixel);
}

vec4 Over(vec4 below, vec4 paint)
{
	return paint + below * (1.0 - paint.a);
}

vec4 Over(vec4 below, vec3 colour, float alpha)
{
	return Over(below, vec4(colour, 1.0) * alpha);
}

vec3 Composite(vec3 base, vec4 paint)
{
	return base * (1.0 - paint.a) + paint.rgb;
}

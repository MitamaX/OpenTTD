uniform usampler2D u_surfaces;
uniform usampler2D u_tiles;
uniform sampler2D u_water;
uniform usampler2D u_network;
uniform usampler2D u_bends;

const float HALF_TILE = 0.5;
const vec2 TILE_CENTRE = vec2(HALF_TILE);
const float MIN_SQUARED_SPAN = 1e-6;
const float MIN_SPREAD = 1e-4;
const float OCTAVE_TURN = 2.39996323;

const vec2 WEST_CORNER = vec2(1.0, 0.0);

const float WATERLINE = 0.58;
const float SHORE_WARP = 0.22;

struct Corners {
	float north;
	float west;
	float east;
	float south;
};

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

/* Value noise with its gradient, smooth enough in both to light bumps with. */
vec3 NoiseSlope(vec2 p)
{
	ivec2 cell = ivec2(floor(p));
	vec2 f = fract(p);
	vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
	vec2 du = 30.0 * f * f * (f * (f - 2.0) + 1.0);
	float a = LatticeHash(cell);
	float b = LatticeHash(cell + ivec2(1, 0));
	float c = LatticeHash(cell + ivec2(0, 1));
	float d = LatticeHash(cell + ivec2(1, 1));
	float twist = a - b - c + d;
	float value = a + (b - a) * u.x + (c - a) * u.y + twist * u.x * u.y;
	return vec3(value, du * (vec2(b - a, c - a) + twist * u.yx));
}

float ResolvedAt(float frequency, float pixels)
{
	return smoothstep(UNRESOLVED_REPEAT_PIXELS, RESOLVED_REPEAT_PIXELS, pixels / frequency);
}

float Resolved(float frequency)
{
	return ResolvedAt(frequency, tile_pixels);
}

/* Each octave is read turned its own way, so the lattice its noise is built on lines up neither with the map nor with the other octaves. */
mat2 OctaveTurn(float frequency)
{
	float angle = frequency * OCTAVE_TURN;
	return mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
}

/* Noise too fine for the pixels to show is left at its mean, and not worked out at all. */
float OctaveAt(vec2 p, float frequency, float pixels)
{
	float shown = ResolvedAt(frequency, pixels);
	return shown <= 0.0 ? 0.5 : mix(0.5, Noise(OctaveTurn(frequency) * p * frequency), shown);
}

float Octave(vec2 p, float frequency)
{
	return OctaveAt(p, frequency, tile_pixels);
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

vec4 WaterTexels(vec2 p)
{
	return textureLod(u_water, p / MapSize(), 0.0);
}

/* The weights a cubic B-spline gives the four texels around a point, f of the way from the second to the third. */
vec4 SplineWeights(float f)
{
	float g = 1.0 - f;
	float w0 = g * g * g / 6.0;
	float w1 = (4.0 - 6.0 * f * f + 3.0 * f * f * f) / 6.0;
	float w3 = f * f * f / 6.0;
	return vec4(w0, w1, 1.0 - w0 - w1 - w3, w3);
}

/* The water texels smoothed by a cubic B-spline, so outlines curve through the tiles instead of running along their edges; four filtered taps stand in for the sixteen texels the spline weighs. */
vec4 SplineWater(vec2 p)
{
	vec2 t = p - TILE_CENTRE;
	vec2 cell = floor(t);
	vec4 wx = SplineWeights(t.x - cell.x);
	vec4 wy = SplineWeights(t.y - cell.y);
	vec2 low = vec2(wx.x + wx.y, wy.x + wy.y);
	vec2 high = vec2(wx.z + wx.w, wy.z + wy.w);
	vec2 a = cell - 1.0 + vec2(wx.y, wy.y) / low + TILE_CENTRE;
	vec2 b = cell + 1.0 + vec2(wx.w, wy.w) / high + TILE_CENTRE;
	vec4 top = low.x * WaterTexels(vec2(a.x, a.y)) + high.x * WaterTexels(vec2(b.x, a.y));
	vec4 bottom = low.x * WaterTexels(vec2(a.x, b.y)) + high.x * WaterTexels(vec2(b.x, b.y));
	return low.y * top + high.y * bottom;
}

/* Natural shores curve and fray a little. */
vec2 ShoreWarp(vec2 p)
{
	return (vec2(Noise(p * 1.7 + 7.1), Noise(p * 1.7 + 93.4)) - 0.5) * 2.0 * SHORE_WARP;
}

/* Canals keep the straight lines of their walls. */
vec4 Walled(vec4 field, vec2 p)
{
	vec4 here = WaterTexels(p);
	return mix(field, here, clamp(here.b * 2.0, 0.0, 1.0));
}

/* The water about a point as level and sea, canal and river shares. */
vec4 WaterField(vec2 p)
{
	return Walled(SplineWater(p + ShoreWarp(p)), p);
}

uvec4 CodesAt(ivec2 tile)
{
	return texelFetch(u_tiles, Clamped(tile), 0);
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

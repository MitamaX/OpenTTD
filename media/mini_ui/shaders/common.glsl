uniform vec2 u_centre;
uniform vec2 u_plane;
uniform float u_ppt;
uniform vec2 u_right;
uniform vec2 u_toward;
uniform vec3 u_sun;
uniform float u_peak;
uniform usampler2D u_surfaces;
uniform usampler2D u_tiles;
uniform sampler2D u_water;
uniform usampler2D u_network;
uniform vec2 u_map;
uniform float u_time;
uniform int u_landscape;
uniform float u_relief;
uniform float u_contour;
uniform float u_grid;
uniform int u_layer;
uniform float u_sink;

in vec2 v_screen;
out vec4 frag_colour;

const float HALF_TILE = 0.5;
const vec2 TILE_CENTRE = vec2(HALF_TILE);
const float SHIFT_PER_LEVEL = VIEW_RISE / VIEW_DEPTH;
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

struct Relief {
	float height;
	vec2 slope;
	vec2 tile_slope;
};

ivec2 near_origin;
uvec4 near_codes[NEIGHBOURHOOD_SPAN * NEIGHBOURHOOD_SPAN];

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

float Cells(vec2 p)
{
	ivec2 cell = ivec2(floor(p));
	vec2 f = fract(p);
	float nearest = 8.0;
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			ivec2 offset = ivec2(x, y);
			vec2 d = vec2(offset) + Hash2(cell + offset) - f;
			nearest = min(nearest, dot(d, d));
		}
	}
	return sqrt(nearest);
}

float Resolved(float frequency)
{
	return smoothstep(UNRESOLVED_REPEAT_PIXELS, RESOLVED_REPEAT_PIXELS, u_ppt / frequency);
}

float ZoomFade(float far_ppt, float near_ppt)
{
	return smoothstep(far_ppt, near_ppt, u_ppt);
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
	return clamp(tile, ivec2(0), ivec2(u_map) - 1);
}

bool OnMap(ivec2 tile)
{
	return all(greaterThanEqual(tile, ivec2(0))) && all(lessThan(tile, ivec2(u_map)));
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
Relief ReliefOn(Corners c, vec2 f)
{
	vec2 north_edges = vec2(c.west - c.north, c.east - c.north);
	vec2 south_edges = vec2(c.south - c.east, c.south - c.west);
	vec2 tile_slope = (north_edges + south_edges) / 2.0;
	if (FoldsWestToEast(c)) {
		vec2 slope = f.x + f.y <= 1.0 ? north_edges : south_edges;
		return Relief(c.west + dot(slope, f - WEST_CORNER), slope, tile_slope);
	}
	vec2 slope = f.x >= f.y ? vec2(north_edges.x, south_edges.y) : vec2(south_edges.x, north_edges.y);
	return Relief(c.north + dot(slope, f), slope, tile_slope);
}

Relief ReliefAt(vec2 p)
{
	vec2 confined = clamp(p, vec2(0.0), u_map);
	ivec2 tile = Clamped(ivec2(floor(confined)));
	return ReliefOn(SurfaceOf(tile), confined - vec2(tile));
}

/* Where the sight line through this level-0 plane point passes at the given level. */
vec2 Sighted(vec2 plane, float level)
{
	return plane + u_toward * (level * SHIFT_PER_LEVEL);
}

/* Where a pixel lies across and down the screen, in tile widths from the screen centre. */
vec2 PixelView(vec2 screen)
{
	return (screen - u_centre) / u_ppt;
}

vec2 PointView(vec2 at, float level)
{
	vec2 offset = at - u_plane;
	return vec2(dot(offset, u_right), dot(offset, u_toward) * VIEW_DEPTH - level * VIEW_RISE);
}

/* No surface rises as fast as the sight line, so closing a share of the gap per step down from the peak never passes the surface nearest the viewer. */
vec2 GroundUnder(vec2 view)
{
	vec2 plane = u_plane + u_right * view.x + u_toward * (view.y / VIEW_DEPTH);
	float level = u_peak;
	for (int iteration = 0; iteration < GROUND_SEARCH_STEPS; iteration++) level = mix(level, ReliefAt(Sighted(plane, level)).height, GROUND_SEARCH_SHARE);
	return Sighted(plane, level);
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

/* The ground point a thing raised this many tiles above the base shows over on screen. */
vec2 Lifted(vec2 base, float rise)
{
	return Sighted(base, -rise / LEVEL_TILES);
}

/* How far up the screen, in tile widths, a height of this many tiles shows. */
float Rise(float tiles)
{
	return tiles / LEVEL_TILES * VIEW_RISE;
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

/* The ground one pixel steps over across and down the screen on the facet with this slope, whatever the pixels beside it show. */
mat2 PixelFootprint(vec2 slope)
{
	vec2 down = u_toward * VIEW_DEPTH - slope * VIEW_RISE;
	return inverse(transpose(mat2(u_right, down))) / u_ppt;
}

/* How wide one screen pixel is on the ground across an edge with this normal. */
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

in vec3 v_position;
in vec3 v_normal;
in vec3 v_grown;
in float v_translucency;
flat in vec2 v_window;

struct Fraying {
	float clumps_per_tile;
	float edge;
	float facing;
};

const Fraying LEAF_FRAYING = Fraying(24.0, 0.55, 0.45);

const float LEAF_COARSE_CLUMPS = 6.0;
const float LEAF_COARSE_SEED = 7.7;

float LeafResolved(float per_tile)
{
	return ResolvedAt(per_tile, TilePixelsAt(v_position));
}

float LeafShare()
{
	return smoothstep(FOLIAGE_TRANSLUCENCY_FROM, FOLIAGE_TRANSLUCENCY_FULL, v_translucency);
}

/* How far a surface is foliage, clumped into sprays of leaves, rather than flesh, sweets or wood, and how far its clumps are still big enough to show. */
float Foliage(Fraying fraying)
{
	return LeafShare() * LeafResolved(fraying.clumps_per_tile);
}

float Bilinear(vec4 corners, vec2 weight)
{
	return mix(mix(corners.x, corners.y, weight.x), mix(corners.z, corners.w, weight.x), weight.y);
}

vec2 BilinearSlope(vec4 corners, vec2 weight)
{
	float twist = corners.x - corners.y - corners.z + corners.w;
	return vec2(corners.y - corners.x + twist * weight.y, corners.z - corners.x + twist * weight.x);
}

vec4 ClumpNoiseSlope(vec3 p)
{
	ivec3 cell = ivec3(floor(p));
	vec3 f = fract(p);
	vec3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
	vec3 du = 30.0 * f * f * (f * (f - 2.0) + 1.0);
	vec4 below = LatticeHashes(cell.xy + cell.z * NOISE_LAYER_STRIDE);
	vec4 above = LatticeHashes(cell.xy + (cell.z + 1) * NOISE_LAYER_STRIDE);
	float low = Bilinear(below, u.xy);
	float high = Bilinear(above, u.xy);
	vec2 across = du.xy * mix(BilinearSlope(below, u.xy), BilinearSlope(above, u.xy), u.z);
	return vec4(mix(low, high, u.z), across, du.z * (high - low));
}

vec4 ClumpField(float per_tile, float seed)
{
	return ClumpNoiseSlope(v_grown * per_tile + seed);
}

const vec4 UNRESOLVED_CLUMPS = vec4(0.5, 0.0, 0.0, 0.0);

vec4 FineClumps(float foliage, Fraying fraying)
{
	return foliage > 0.0 ? ClumpField(fraying.clumps_per_tile, 0.0) : UNRESOLVED_CLUMPS;
}

float Clumps(float foliage, Fraying fraying)
{
	return FineClumps(foliage, fraying).x;
}

vec4 CoarseClumps()
{
	return LeafResolved(LEAF_COARSE_CLUMPS) > 0.0 ? ClumpField(LEAF_COARSE_CLUMPS, LEAF_COARSE_SEED) : UNRESOLVED_CLUMPS;
}

float CoarseClumpsShown()
{
	return LeafShare() * LeafResolved(LEAF_COARSE_CLUMPS);
}

void DropOutOfWindow()
{
	bool outside = !InWindow(v_window, ScreenNoise(gl_FragCoord.xy));
	if (Dropped(outside)) discard;
}

float FrayingReach(vec3 normal, vec3 view, float foliage, Fraying fraying)
{
	float edge_on = 1.0 - smoothstep(0.0, fraying.facing, abs(dot(normal, view)));
	return fraying.edge * edge_on * foliage;
}

void DropFrayed(float reach, float clumps)
{
	if (Dropped(clumps < reach)) discard;
}

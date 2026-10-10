in vec3 v_position;
in vec3 v_normal;
in vec3 v_grown;
in float v_translucency;
flat in vec2 v_window;

const vec2 LEAF_TRANSLUCENCY = vec2(0.2, 0.4);
const float LEAF_CLUMPS_PER_TILE = 24.0;
const float RAGGED_EDGE = 0.55;
const float RAGGED_FACING = 0.45;

float LeafResolved(float per_tile)
{
	return ResolvedAt(per_tile, TilePixelsAt(distance(Eye(), v_position)));
}

float LeafShare()
{
	return smoothstep(LEAF_TRANSLUCENCY.x, LEAF_TRANSLUCENCY.y, v_translucency);
}

/* How far a surface is foliage, clumped into sprays of leaves, rather than flesh, sweets or wood, and how far its clumps are still big enough to show. */
float Foliage()
{
	return LeafShare() * LeafResolved(LEAF_CLUMPS_PER_TILE);
}

float Clumps(float foliage)
{
	return foliage > 0.0 ? Noise(v_grown * LEAF_CLUMPS_PER_TILE) : 0.5;
}

void DropOutOfWindow()
{
	bool outside = !InWindow(v_window, ScreenNoise(gl_FragCoord.xy));
	if (Dropped(outside)) discard;
}

void DropFrayed(vec3 normal, vec3 view, float clumps, float foliage)
{
	float edge_on = 1.0 - smoothstep(0.0, RAGGED_FACING, abs(dot(normal, view)));
	bool frayed = clumps < RAGGED_EDGE * edge_on * foliage;
	if (Dropped(frayed)) discard;
}

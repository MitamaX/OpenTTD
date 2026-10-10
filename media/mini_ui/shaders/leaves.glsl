in vec3 v_position;
in vec3 v_normal;
in vec3 v_grown;
in float v_translucency;
flat in vec2 v_window;

const vec2 LEAF_TRANSLUCENCY = vec2(0.2, 0.4);

struct Fraying {
	float clumps_per_tile;
	float edge;
	float facing;
};

const Fraying LEAF_FRAYING = Fraying(24.0, 0.55, 0.45);

float LeafResolved(float per_tile)
{
	return ResolvedAt(per_tile, TilePixelsAt(distance(Eye(), v_position)));
}

float LeafShare()
{
	return smoothstep(LEAF_TRANSLUCENCY.x, LEAF_TRANSLUCENCY.y, v_translucency);
}

/* How far a surface is foliage, clumped into sprays of leaves, rather than flesh, sweets or wood, and how far its clumps are still big enough to show. */
float Foliage(Fraying fraying)
{
	return LeafShare() * LeafResolved(fraying.clumps_per_tile);
}

float Clumps(float foliage, Fraying fraying)
{
	return foliage > 0.0 ? Noise(v_grown * fraying.clumps_per_tile) : 0.5;
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

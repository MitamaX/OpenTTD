int TreeCount(uvec4 codes)
{
	return int(codes.b & FLORA_COUNT_MASK);
}

/* How much of a tile its trees' crowns cover, by how many stand on it and how grown they are. */
float FloraCover(uvec4 codes)
{
	float grown = TREE_AGE_SCALES[(codes.b >> FLORA_AGE_SHIFT) & FLORA_AGE_MASK] / TREE_AGE_SCALES[TREE_GROWN];
	return min(float(TreeCount(codes)) / float(FLORA_MOST_TREES), 1.0) * grown * grown;
}

/* How far a tree this many tile pixels across has gone over to the near side of a detail's floor, across the band the two details crossfade over. */
float FloorCrossing(float tile_pixels, int detail)
{
	float octave = log2(max(tile_pixels, 1e-6));
	return clamp((octave - log2(TREE_DETAIL_FLOORS[detail]) + TREE_CROSSFADE) / (2.0 * TREE_CROSSFADE), 0.0, 1.0);
}

/* A tree shows at a detail on the pixels whose screen noise falls within this window: it fades in past the finer detail's floor and out past its own. */
vec2 DetailWindow(float tile_pixels, int detail)
{
	float enter = detail == 0 ? 0.0 : FloorCrossing(tile_pixels, detail - 1);
	return vec2(enter, FloorCrossing(tile_pixels, detail));
}

bool InWindow(vec2 window, float noise)
{
	return noise >= window.x && noise < window.y;
}

/* The share of a forest the ground's tint stands in for, where even the coarsest trees have faded out. */
float ForestTint(float tile_pixels)
{
	return 1.0 - FloorCrossing(tile_pixels, TREE_DETAILS - 1);
}

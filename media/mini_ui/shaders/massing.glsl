uniform usampler2D u_massing;

const uvec3 RGB565_SHIFTS = uvec3(11u, 5u, 0u);
const uvec3 RGB565_MASKS = uvec3(31u, 63u, 31u);
const uvec4 MASSING_INSET_SHIFTS = MASSING_INSET_SHIFT + uvec4(0u, 1u, 2u, 3u) * MASSING_INSET_BITS;

struct Box {
	vec2 low;
	vec2 high;
	float foot;
	float top;
};

struct Massing {
	Box box;
	float eaves;
	vec3 wall;
	vec3 roof;
	uint grid;
};

vec3 Rgb565(uint bits)
{
	return vec3((uvec3(bits) >> RGB565_SHIFTS) & RGB565_MASKS) / vec3(RGB565_MASKS);
}

ivec2 AnchorOf(ivec2 tile, uint traits)
{
	return tile - ivec2(notEqual(uvec2(traits) & uvec2(MASSING_FAR_X_BIT, MASSING_FAR_Y_BIT), uvec2(0u)));
}

ivec2 AnchorAt(ivec2 tile)
{
	return AnchorOf(tile, texelFetch(u_massing, Clamped(tile), 0).w);
}

float FormRise(uint texels)
{
	return float(texels) / TEXELS * FORM_HEIGHT_SCALE;
}

Box BoxOf(ivec2 tile, uvec4 texel, float lowest)
{
	vec4 insets = vec4((uvec4(texel.w) >> MASSING_INSET_SHIFTS) & MASSING_INSET_MOST) / MASSING_INSET_STEPS;
	float foot = (lowest + float((texel.w >> MASSING_RAISED_SHIFT) & MASSING_RAISED_MOST)) * LevelRise();
	vec2 corner = vec2(tile);
	return Box(corner + insets.xz, corner + 1.0 - insets.yw, foot, foot + FormRise(texel.x & MASSING_LOW_BYTE));
}

Massing MassingOf(Box box, uvec4 texel)
{
	return Massing(box, box.foot + FormRise(texel.x >> MASSING_EAVES_SHIFT), Rgb565(texel.y), Rgb565(texel.z), texel.w & MASSING_GRID_MASK);
}

const uint SEEN_NOTHING = 0u;
const uint SEEN_WALL = 1u;
const uint SEEN_ROOF = 2u;
const uint SEEN_GROUND = 3u;
const int MASSING_CELLS = 6;
const float GROUND_TOUCH = 1e-3;

struct Sighting {
	uint seen;
	vec3 point;
	vec3 normal;
	float run;
	ivec2 tile;
};

Massing MassingAt(ivec2 tile)
{
	uvec4 texel = texelFetch(u_massing, tile, 0);
	return MassingOf(BoxOf(tile, texel, LowestCorner(SurfaceOf(tile))), texel);
}

struct Strike {
	uint seen;
	float run;
	vec2 across;
};

const Strike NO_STRIKE = Strike(SEEN_NOTHING, 0.0, vec2(0.0));

Strike MetBox(vec3 from, vec3 way, vec2 span, Box box, vec2 inverse)
{
	vec2 toward = step(0.0, inverse);
	vec2 entering = (mix(box.high, box.low, toward) - from.xy) * inverse;
	vec2 leaving = (mix(box.low, box.high, toward) - from.xy) * inverse;
	float enter = max(max(entering.x, entering.y), span.x);
	float leave = min(min(leaving.x, leaving.y), span.y);
	if (enter > leave) return NO_STRIKE;
	if (from.z + way.z * enter <= box.top) return Strike(SEEN_WALL, enter, entering.x > entering.y ? vec2(-sign(inverse.x), 0.0) : vec2(0.0, -sign(inverse.y)));
	if (way.z >= 0.0) return NO_STRIKE;
	float onto = (box.top - from.z) / way.z;
	return onto > leave ? NO_STRIKE : Strike(SEEN_ROOF, onto, vec2(0.0));
}

float MetGround(vec3 from, vec3 way, vec2 span, Corners ground, ivec2 tile)
{
	vec3 first = from + way * span.x;
	vec3 last = from + way * span.y;
	if (min(first.z, last.z) >= HighestCorner(ground) * LevelRise() + GROUND_TOUCH) return -1.0;
	float above = first.z - FacetLevel(ground, clamp(first.xy - vec2(tile), 0.0, 1.0)) * LevelRise();
	float below = last.z - FacetLevel(ground, clamp(last.xy - vec2(tile), 0.0, 1.0)) * LevelRise();
	if (above < 0.0 || below >= GROUND_TOUCH) return -1.0;
	return mix(span.x, span.y, clamp(above / max(above - below, LEAST_DIVISOR), 0.0, 1.0));
}

Sighting Traced(vec3 from, vec3 way, ivec2 own)
{
	vec2 toward = vec2(greaterThan(way.xy, vec2(0.0)));
	ivec2 stride = ivec2(sign(way.xy));
	vec2 per_cell = 1.0 / max(abs(way.xy), vec2(LEAST_ACROSS));
	vec2 inverse = mix(-per_cell, per_cell, toward);
	ivec2 cell = ivec2(floor(from.xy));
	vec2 next = abs(vec2(cell) + toward - from.xy) * per_cell;
	float ceiling = Peak() * LevelRise() + TALLEST_STRUCTURE;
	float enter = 0.0;
	Strike struck = NO_STRIKE;
	for (int crossed = 0; crossed < MASSING_CELLS; crossed++) {
		if (!OnMap(cell) || from.z + way.z * enter > ceiling) break;
		bool across_x = next.x < next.y;
		float leave = across_x ? next.x : next.y;
		uvec4 texel = texelFetch(u_massing, cell, 0);
		Corners ground = SurfaceOf(cell);
		float lowest = LowestCorner(ground);
		float sunk = way.z < 0.0 ? (lowest * LevelRise() - from.z) / way.z : leave;
		vec2 span = vec2(enter, min(leave, sunk));
		bool standing = texel.x != 0u && AnchorOf(cell, texel.w) != own;
		struck = standing ? MetBox(from, way, span, BoxOf(cell, texel, lowest), inverse) : NO_STRIKE;
		float landed = MetGround(from, way, span, ground, cell);
		if (landed >= 0.0 && (struck.seen == SEEN_NOTHING || landed < struck.run)) struck = Strike(SEEN_GROUND, landed, vec2(0.0));
		if (struck.seen != SEEN_NOTHING || sunk < leave) break;
		enter = leave;
		cell += across_x ? ivec2(stride.x, 0) : ivec2(0, stride.y);
		next += across_x ? vec2(per_cell.x, 0.0) : vec2(0.0, per_cell.y);
	}
	return Sighting(struck.seen, from + way * struck.run, struck.seen == SEEN_WALL ? vec3(struck.across, 0.0) : UP, struck.run, cell);
}

uniform vec2 u_fade;

const vec2 LATTICE_BAYS = vec2(5.0, 7.0);
const float LATTICE_MEMBER = 0.1;
const float LATTICE_BRACE = 0.07;

/* How sharply a pattern repeating once a unit of x still shows: whole while a repeat spans a few pixels, gone once it blurs. */
float Sharpness(float x)
{
	return SharpnessAt(fwidth(x));
}

float Sharpness(vec2 q)
{
	return min(Sharpness(q.x), Sharpness(q.y));
}

/* How much of a pixel lines of this width, one a unit, cover; lines too fine to show blend into the share they cover. */
float Lines(float x, float width, float sharpness)
{
	return mix(width, LineCover(x, width, fwidth(x)), sharpness);
}

float Lines(float x, float width)
{
	return Lines(x, width, Sharpness(x));
}

float Band(float x, float low, float high)
{
	return Band(x, low, high, fwidth(x));
}

/* The members of a braced frame, as a share of the point they cover: uprights and ledgers in bays, every other bay braced across from corner to corner. */
float LatticeCover(vec2 p)
{
	vec2 cell = p * LATTICE_BAYS;
	vec2 bay = floor(cell);
	float braced = step(1.0, mod(bay.x + bay.y, 2.0));
	return max(max(Lines(cell.x, LATTICE_MEMBER), Lines(cell.y, LATTICE_MEMBER)), Lines(cell.x - cell.y, LATTICE_BRACE) * braced);
}

/* The pickets of a fence and the two rails they are nailed to. */
float PicketCover(vec2 p)
{
	return max(Lines(p.x * 45.0, 0.5), max(Band(p.y, 0.006, 0.0095), Band(p.y, 0.017, 0.0205)));
}

/* How much of a point an open cladding covers; any other cladding covers it whole. */
float Cover(uint material, vec2 p)
{
	switch (material) {
		case CLAD_LATTICE: return LatticeCover(p);
		case CLAD_PICKETS: return PicketCover(p);
		default: return 1.0;
	}
}

void DropUnseen(float pixels, uint material, vec2 pattern)
{
	float noise = ScreenNoise(gl_FragCoord.xy);
	bool gap = Cover(material, pattern) < noise;
	bool faded = smoothstep(u_fade.x, u_fade.y, pixels) <= noise;
	if (Dropped(faded || gap)) discard;
}

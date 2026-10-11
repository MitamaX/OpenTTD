const int MIRRORED_CLOUD_OCTAVES = 3;
const vec2 STREET_WIDTHS = vec2(1.0, 2.4);
const int SIDE_KEY_STRIDE = 3;
const float STREET_SUNLIT = 0.5;
const float PAVEMENT_WIDTH = 0.2;
const vec3 DISTANT_TOWN = vec3(0.16, 0.17, 0.15);
const float LEAST_RISE = 0.05;
const float STREET_STEP = 0.05;
const float FRONTAGE_BUILDING_WIDTH = 0.9;
const vec2 FRONTAGE_HEIGHTS = vec2(0.25, 2.6);
const float FRONTAGE_SHADED_SHARE = 0.5;
const float FRONTAGE_STOREY = 0.13;
const float FRONTAGE_RIBBON = 0.45;
const float FRONTAGE_BAY = 0.07;
const vec2 FRONTAGE_GLAZED_BAYS = vec2(0.4, 0.85);
const float FRONTAGE_GLAZING_CONTRAST = 0.6;
const float FRONTAGE_SATURATION = 0.2;
const float FRONTAGE_STREET_OPENNESS = 0.45;
const float FRONTAGE_CANYON_DEPTH = 0.8;
const ivec2 FRONTAGE_SALT = ivec2(5923, 3301);
const vec3 TOWN_WALL_PAINTS[] = TOWN_WALL_TONES;
const vec3 CANDY_PAINTS[] = CANDY_TONES;

struct Street {
	vec3 point;
	vec2 outward;
	float ground;
	float width;
};

struct Frontage {
	float height;
	vec3 paint;
	float sunlit;
	float glazed;
};

float GroundBelow(vec2 at)
{
	ivec2 tile = Clamped(ivec2(floor(at)));
	return FacetLevel(SurfaceOf(tile), clamp(at - vec2(tile), 0.0, 1.0)) * LevelRise();
}

/* The street a face looks out on, its ground read just in front of the face and its width rolled for each side of the building. */
Street StreetBefore(vec3 point, vec3 face, uint seed)
{
	float level = length(face.xy);
	vec2 outward = level > LEAST_ACROSS ? face.xy / level : vec2(0.0);
	ivec2 side = ivec2(round(outward));
	float width = mix(STREET_WIDTHS.x, STREET_WIDTHS.y, Hash(ivec2(int(seed), side.x + SIDE_KEY_STRIDE * side.y)));
	return Street(point, outward, GroundBelow(point.xy + outward * STREET_STEP), width);
}

vec3 WallPaint(float roll)
{
	return Landscape() == LANDSCAPE_TOYLAND ? CANDY_PAINTS[Pick(roll, CANDY_PAINTS.length())] : TOWN_WALL_PAINTS[Pick(roll, TOWN_WALL_PAINTS.length())];
}

Frontage FrontageOf(ivec2 building)
{
	vec2 shape = Hash2(building);
	vec2 look = Hash2(building + FRONTAGE_SALT);
	return Frontage(mix(FRONTAGE_HEIGHTS.x, FRONTAGE_HEIGHTS.y, shape.x), WallPaint(shape.y), look.x < FRONTAGE_SHADED_SHARE ? 0.0 : 1.0, mix(FRONTAGE_GLAZED_BAYS.x, FRONTAGE_GLAZED_BAYS.y, look.y));
}

Frontage Blended(Frontage a, Frontage b, float share)
{
	return Frontage(mix(a.height, b.height, share), mix(a.paint, b.paint, share), mix(a.sunlit, b.sunlit, share), mix(a.glazed, b.glazed, share));
}

Frontage FrontageAt(float along, float line, float blur)
{
	float place = along / FRONTAGE_BUILDING_WIDTH;
	float from_middle = fract(place) - 0.5;
	ivec2 building = ivec2(floor(place), floor(line));
	ivec2 neighbour = building + ivec2(from_middle < 0.0 ? -1 : 1, 0);
	float to_edge = (0.5 - abs(from_middle)) * FRONTAGE_BUILDING_WIDTH;
	return Blended(FrontageOf(building), FrontageOf(neighbour), 0.5 - 0.5 * smoothstep(0.0, 0.5 * blur, to_edge));
}

vec3 FrontageSeen(Frontage frontage, vec2 at, float blur, vec2 outward)
{
	float windows = LinesAt(at.x / FRONTAGE_BAY, frontage.glazed, blur / FRONTAGE_BAY) * LinesAt(at.y / FRONTAGE_STOREY, FRONTAGE_RIBBON, blur / FRONTAGE_STOREY);
	float openness = mix(FRONTAGE_STREET_OPENNESS, 1.0, smoothstep(0.0, FRONTAGE_CANYON_DEPTH, at.y));
	vec3 facing = vec3(-outward, 0.0);
	vec3 wall = Saturated(Linear(frontage.paint), FRONTAGE_SATURATION);
	return mix(wall, Linear(DIM_ROOM), windows * FRONTAGE_GLAZING_CONTRAST) * (Sunlight(facing, frontage.sunlit) + AmbientLight(facing) * openness);
}

vec3 StreetSeen(Street street, vec3 way)
{
	float drop = street.point.z - street.ground;
	float out_from_face = dot(way.xy, street.outward) * drop / max(-way.z, LEAST_ACROSS);
	bool kerbside = min(out_from_face, street.width - out_from_face) < PAVEMENT_WIDTH;
	return Linear(kerbside ? PAVEMENT_TONE : DISTANT_ROAD) * (Sunlight(UPWARD, STREET_SUNLIT) + AmbientLight(UPWARD));
}

vec3 Beyond(Street street, vec3 way)
{
	if (way.z >= 0.0) return CloudedSky(way, MIRRORED_CLOUD_OCTAVES);
	float rise = max(street.point.z - street.ground, LEAST_RISE);
	vec3 town = DISTANT_TOWN * (Sunlight(UPWARD, 1.0) + AmbientLight(UPWARD));
	return Hazed(town, way, street.point, street.point + way * (rise / -way.z));
}

vec4 Opposite(Street street, vec3 way, float roughness)
{
	float across = dot(way.xy, street.outward);
	if (across <= LEAST_ACROSS) return vec4(0.0);
	float run = street.width / across;
	vec3 hit = street.point + way * run;
	float above = hit.z - street.ground;
	if (above < 0.0) return vec4(StreetSeen(street, way), 1.0);
	float blur = 1.0 / TilePixelsAt(distance(Eye(), street.point) + run) + roughness * roughness * run;
	vec2 at = vec2(dot(hit.xy, vec2(-street.outward.y, street.outward.x)), above);
	Frontage frontage = FrontageAt(at.x, dot(hit.xy, street.outward), blur);
	float cover = 1.0 - smoothstep(-0.5 * blur, 0.5 * blur, above - frontage.height);
	return vec4(FrontageSeen(frontage, at, blur, street.outward), 1.0) * cover;
}

vec3 Mirrored(Street street, vec3 way, float roughness)
{
	vec4 opposite = Opposite(street, way, roughness);
	return opposite.a >= 1.0 ? opposite.rgb : Composite(Beyond(street, way), opposite);
}

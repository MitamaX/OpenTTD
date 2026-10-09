const float PANE_REFLECTANCE = 0.08;
const float ROOM_SUNLIT_SHARE = 0.12;

const int MIRRORED_CLOUD_OCTAVES = 3;
const vec3 UPWARD = vec3(0.0, 0.0, 1.0);
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
const float FRONTAGE_GLAZING_CONTRAST = 0.35;
const float FRONTAGE_SATURATION = 0.35;
const vec2 FRONTAGE_RESOLVED_STOREYS = vec2(0.25, 0.6);
const ivec2 FRONTAGE_SALT = ivec2(5923, 3301);
const vec3 TOWN_WALL_PAINTS[] = TOWN_WALL_TONES;
const vec3 CANDY_PAINTS[] = CANDY_TONES;

struct Pane {
	vec3 room;
	vec3 film;
	float reflectance;
	float daylight;
	vec3 lamp;
};

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

Frontage FrontageAt(float along, float line)
{
	ivec2 building = ivec2(floor(along / FRONTAGE_BUILDING_WIDTH), floor(line));
	vec2 shape = Hash2(building);
	float shaded = Hash(building + FRONTAGE_SALT);
	return Frontage(mix(FRONTAGE_HEIGHTS.x, FRONTAGE_HEIGHTS.y, shape.x), WallPaint(shape.y), shaded < FRONTAGE_SHADED_SHARE ? 0.0 : 1.0);
}

vec3 FrontageSeen(Frontage frontage, float above, float footprint, vec2 outward)
{
	float ribbon = abs(fract(above / FRONTAGE_STOREY) - 0.5) * 2.0 < FRONTAGE_RIBBON ? 1.0 : 0.0;
	float shown = 1.0 - smoothstep(FRONTAGE_RESOLVED_STOREYS.x, FRONTAGE_RESOLVED_STOREYS.y, footprint / FRONTAGE_STOREY);
	float glazed = mix(FRONTAGE_RIBBON, ribbon, shown);
	vec3 facing = vec3(-outward, 0.0);
	vec3 wall = Saturated(Linear(frontage.paint), FRONTAGE_SATURATION);
	return mix(wall, Linear(DIM_ROOM), glazed * FRONTAGE_GLAZING_CONTRAST) * (Sunlight(facing, frontage.sunlit) + AmbientLight(facing));
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

vec3 Mirrored(Street street, vec3 way)
{
	float across = dot(way.xy, street.outward);
	if (across <= LEAST_ACROSS) return Beyond(street, way);
	float run = street.width / across;
	vec3 hit = street.point + way * run;
	float above = hit.z - street.ground;
	if (above < 0.0) return StreetSeen(street, way);
	float along = dot(hit.xy, vec2(-street.outward.y, street.outward.x));
	Frontage frontage = FrontageAt(along, dot(hit.xy, street.outward));
	if (above >= frontage.height) return Beyond(street, way);
	return FrontageSeen(frontage, above, 1.0 / TilePixelsAt(distance(Eye(), street.point) + run), street.outward);
}

vec4 PaneRadiance(Pane pane, Street street, vec3 normal, vec3 view, float roughness, float occlusion, float sunlit)
{
	Coat coat = Coat(roughness, pane.reflectance);
	vec3 sun = Sunlight(normal, sunlit);
	vec3 ambient = AmbientLight(normal);
	float mirrored = SurroundingReflectance(coat, max(dot(normal, view), 0.0));
	vec3 through = Diffuse(pane.room, (sun * ROOM_SUNLIT_SHARE + ambient * occlusion) * pane.daylight + pane.lamp, mirrored);
	vec3 surroundings = Mirrored(street, reflect(-view, normal)) * mirrored * occlusion;
	return vec4(through + pane.film * (surroundings + sun * Highlight(normal, view, SunDirection(), coat)), AmbientShare(ambient, sun));
}

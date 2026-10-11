const int MIRRORED_CLOUD_OCTAVES = 3;
const vec3 DISTANT_TOWN = vec3(0.16, 0.17, 0.15);
const float LEAST_RISE = 0.05;
const float MIRRORED_WINDOW_CONTRAST = 0.6;
const float MIRRORED_SATURATION = 0.2;
const float MIRRORED_STREET_OPENNESS = 0.45;
const float MIRRORED_CANYON_DEPTH = 0.8;
const vec2 MIRRORED_TRACED_PIXELS = vec2(12.0, 24.0);
const float MIRRORED_LIFT = 0.01;
const float MIRRORED_SHADOW_LIFT = 0.1;
const float MIRRORED_GLAZED_BODY = 0.35;
const float MIRRORED_WINDOW_SHEEN = 0.15;

struct Seen {
	vec3 albedo;
	vec3 normal;
	float openness;
	vec3 sheen;
	float cover;
};

float GroundBelow(vec2 at)
{
	ivec2 tile = Clamped(ivec2(floor(at)));
	return FacetLevel(SurfaceOf(tile), clamp(at - vec2(tile), 0.0, 1.0)) * LevelRise();
}

vec3 Beyond(vec3 from, vec3 way)
{
	if (way.z >= 0.0) return CloudedSky(way, MIRRORED_CLOUD_OCTAVES);
	float rise = max(from.z - GroundBelow(from.xy), LEAST_RISE);
	vec3 town = DISTANT_TOWN * (Sunlight(UPWARD, 1.0) + AmbientLight(UPWARD));
	return Hazed(town, way, from, from + way * (rise / -way.z));
}

float CanyonOpenness(float above_foot)
{
	return mix(MIRRORED_STREET_OPENNESS, 1.0, smoothstep(0.0, MIRRORED_CANYON_DEPTH, above_foot));
}

Seen WallSeen(Sighting sighting, float blur)
{
	Massing massing = MassingOf(sighting.box, sighting.texel);
	uint grid = massing.grid;
	vec3 point = sighting.point;
	vec3 normal = sighting.normal;
	float pixel = blur * TEXELS;
	float pitch = FACADE_PITCH[grid];
	float along = dot(point.xy, vec2(-normal.y, normal.x)) * TEXELS;
	float up = (point.z - massing.box.foot) / FORM_HEIGHT_SCALE * TEXELS - FACADE_GROUND[grid];
	bool upper = up >= 0.0;
	Opening opening = upper ? UPPER_OPENINGS[grid] : GROUND_OPENINGS[grid];
	float row = upper ? mod(up, FACADE_STOREY[grid]) : up + FACADE_GROUND[grid];
	bool walled = point.z < massing.eaves;
	float columns = LinesAt(along / pitch - 0.5, opening.width / pitch, pixel / pitch);
	float windows = walled && opening.kind != OPEN_NONE ? columns * Band(row, opening.sill, opening.sill + opening.height, pixel) : 0.0;
	vec3 face = Saturated(Linear(walled ? massing.wall : massing.roof), MIRRORED_SATURATION);
	vec3 pane = grid == WINDOWS_CURTAIN ? Linear(massing.wall) * MIRRORED_GLAZED_BODY : Linear(DIM_ROOM);
	float cover = 1.0 - smoothstep(-0.5 * blur, 0.5 * blur, point.z - massing.box.top);
	return Seen(mix(face, pane, windows * MIRRORED_WINDOW_CONTRAST), normal, CanyonOpenness(point.z - massing.box.foot), SKY_LIGHT * MIRRORED_WINDOW_SHEEN * windows, cover);
}

Seen RoofSeen(Sighting sighting)
{
	bool snowed = Landscape() == LANDSCAPE_ARCTIC && sighting.point.z / LevelRise() > SnowLine();
	return Seen(Linear(snowed ? SNOW : Rgb565(sighting.texel.z)), UPWARD, 1.0, vec3(0.0), 1.0);
}

Seen GroundSeen(Sighting sighting)
{
	return Seen(Linear(DISTANT_ROAD), UPWARD, MIRRORED_STREET_OPENNESS, vec3(0.0), 1.0);
}

Seen SeenAt(Sighting sighting, float blur)
{
	if (sighting.seen == SEEN_WALL) return WallSeen(sighting, blur);
	if (sighting.seen == SEEN_ROOF) return RoofSeen(sighting);
	return GroundSeen(sighting);
}

vec3 LitAt(Seen seen, vec3 point, float clouds)
{
	float sunlit = dot(seen.normal, SunDirection()) > 0.0 ? SunReaches(point + seen.normal * MIRRORED_SHADOW_LIFT) * clouds : 0.0;
	return seen.albedo * (Sunlight(seen.normal, sunlit) + AmbientLight(seen.normal) * seen.openness) + seen.sheen;
}

vec3 Mirrored(vec3 point, vec3 face, vec3 way, ivec2 own, float roughness, float clouds, bool traces)
{
	vec3 from = point + face * MIRRORED_LIFT;
	float far = distance(Eye(), from);
	float traced = traces ? smoothstep(MIRRORED_TRACED_PIXELS.x, MIRRORED_TRACED_PIXELS.y, TilePixelsAt(far)) : 0.0;
	vec4 near = vec4(0.0);
	if (traced > 0.0) {
		Sighting sighting = Traced(from, way, own);
		if (sighting.seen != SEEN_NOTHING) {
			float blur = 1.0 / TilePixelsAt(far + sighting.run) + roughness * roughness * sighting.run;
			Seen seen = SeenAt(sighting, blur);
			near = vec4(Hazed(LitAt(seen, sighting.point, clouds), way, from, sighting.point), 1.0) * seen.cover * traced;
		}
	}
	return near.a >= 1.0 ? near.rgb : Composite(Beyond(from, way), near);
}

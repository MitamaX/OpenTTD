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
const float MIRRORED_WATER_SHEEN = 0.3;
const float MIRRORED_GRASS_TONE = 0.5;
const vec2 MIRRORED_REACH_CELLS = float(MASSING_CELLS) * vec2(0.5, 1.0);

struct Seen {
	vec3 albedo;
	vec3 normal;
	float openness;
	vec3 sheen;
	float cover;
};

float GroundBelow(vec2 at)
{
	return LowestCorner(SurfaceOf(ivec2(floor(at)))) * LevelRise();
}

float RunToGround(vec3 from, vec3 way)
{
	return max(from.z - GroundBelow(from.xy), LEAST_RISE) / -way.z;
}

vec3 Beyond(vec3 from, vec3 way)
{
	if (way.z >= 0.0) return CloudedSky(way, MIRRORED_CLOUD_OCTAVES);
	vec3 town = DISTANT_TOWN * (Sunlight(UP, 1.0) + AmbientLight(UP));
	return Hazed(town, way, from, from + way * RunToGround(from, way));
}

Sighting Landed(vec3 from, vec3 way)
{
	if (way.z >= 0.0) return Unsighted(from, ivec2(floor(from.xy)));
	float run = RunToGround(from, way);
	vec3 point = from + way * run;
	return Sighting(SEEN_GROUND, point, UP, run, Clamped(ivec2(floor(point.xy))));
}

float WithinReach(vec3 way, float run)
{
	return 1.0 - smoothstep(MIRRORED_REACH_CELLS.x, MIRRORED_REACH_CELLS.y, run * (abs(way.x) + abs(way.y)));
}

float CanyonOpenness(float above_foot)
{
	return mix(MIRRORED_STREET_OPENNESS, 1.0, smoothstep(0.0, MIRRORED_CANYON_DEPTH, above_foot));
}

Seen WallSeen(Sighting sighting, Massing massing, float blur)
{
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

Seen RoofSeen(Sighting sighting, Massing massing)
{
	bool snowed = Landscape() == LANDSCAPE_ARCTIC && sighting.point.z / LevelRise() > SnowLine();
	return Seen(Linear(snowed ? SNOW : massing.roof), UP, 1.0, vec3(0.0), 1.0);
}

bool Along(uint ends, uint way, float across)
{
	return (ends & way) == way && abs(across) < ROAD_HALF;
}

vec3 CoverPaint(uint material)
{
	if (material == MAT_SNOW) return Linear(SNOW);
	if (material == MAT_DESERT) return Linear(SAND);
	if (material == MAT_PAVED) return Linear(PAVEMENT_TONE);
	if (material == MAT_GRASS || material == MAT_ROUGH) return Linear(mix(GRASS_DARK, GRASS_LIGHT, MIRRORED_GRASS_TONE));
	return GROUND_ALBEDOS[Landscape()];
}

Seen GroundSeen(Sighting sighting, vec3 way)
{
	ivec2 tile = sighting.tile;
	vec2 f = sighting.point.xy - vec2(tile) - TILE_CENTRE;
	uvec4 network = texelFetch(u_network, Clamped(tile), 0);
	vec3 albedo = CoverPaint(CodesAt(tile).x);
	vec3 sheen = vec3(0.0);
	if (Along(network.y, ALONG_X_ENDS, f.y) || Along(network.y, ALONG_Y_ENDS, f.x)) {
		albedo = Linear(DISTANT_ROAD);
	} else if ((network.w & NETWORK_KERB_BIT) != 0u) {
		albedo = Linear(PAVEMENT_TONE);
	} else if (network.x != 0u) {
		albedo = Linear(DISTANT_BALLAST);
	} else if (TileWater(tile).r > HALF_TILE) {
		albedo = Linear(DISTANT_WATER);
		sheen = Surroundings(reflect(way, UP)) * MIRRORED_WATER_SHEEN;
	}
	return Seen(albedo, UP, MIRRORED_STREET_OPENNESS, sheen, 1.0);
}

Seen SeenAt(Sighting sighting, vec3 way, float blur)
{
	if (sighting.seen == SEEN_GROUND) return GroundSeen(sighting, way);
	Massing massing = MassingAt(sighting.tile);
	return sighting.seen == SEEN_WALL ? WallSeen(sighting, massing, blur) : RoofSeen(sighting, massing);
}

vec3 LitAt(Seen seen, vec3 point, float clouds)
{
	float sunlit = dot(seen.normal, SunDirection()) > 0.0 ? SunReaches(point + seen.normal * MIRRORED_SHADOW_LIFT) * clouds : 0.0;
	return seen.albedo * (Sunlight(seen.normal, sunlit) + AmbientLight(seen.normal) * seen.openness) + seen.sheen;
}

vec3 Mirrored(vec3 point, vec3 face, vec3 way, ivec2 own, float roughness, float clouds)
{
	vec3 from = point + face * MIRRORED_LIFT;
	float far = distance(Eye(), from);
	float traced = smoothstep(MIRRORED_TRACED_PIXELS.x, MIRRORED_TRACED_PIXELS.y, TilePixelsAt(far));
	vec4 near = vec4(0.0);
	if (traced > 0.0) {
		Sighting sighting = own != NO_ANCHOR ? Traced(from, way, own) : Landed(from, way);
		float reach = WithinReach(way, sighting.run);
		if (sighting.seen != SEEN_NOTHING && reach > 0.0) {
			float blur = 1.0 / TilePixelsAt(far + sighting.run) + roughness * roughness * sighting.run;
			Seen seen = SeenAt(sighting, way, blur);
			near = vec4(Hazed(LitAt(seen, sighting.point, clouds), way, from, sighting.point), 1.0) * seen.cover * traced * reach;
		}
	}
	return near.a >= 1.0 ? near.rgb : Composite(Beyond(from, way), near);
}

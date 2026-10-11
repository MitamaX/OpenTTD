uniform float u_contour;
uniform float u_grid;

in vec3 v_world;
in vec3 v_normal;
in float v_mark;

out vec4 frag_colour;

const float STANDARD_SLOPE = 1.0;
const float STEEPEST_SLOPE = sqrt(2.0);
const float ALPINE_ALTITUDE = 0.55;
const float STEEP_BARING = 1.1;
const float SHED_BARING = 0.9;
const float ALPINE_BARING = 0.9;
const float SCREE_EXPOSURE = 0.5;
const float TOY_BARING = 0.55;
const float ROCK_EXPOSURE = 0.8;
const float EXPOSURE_BLEND = 0.14;
const float BLANKET_SLIP_SLOPE = 1.1;
const float SNOW_EDGE = 0.12;
const float SAND_EDGE = 0.2;
const float BLANKET_FRAY = 0.35;
const float BLANKET_FRAY_FREQUENCY = 0.6;
const float BLANKET_MEANDER = 0.6;
const float CLINGING = 0.75;
const float SNOW_LINE_RAGGED = 2.5;
const float CAP_COVER = 0.85;
const vec3 CAP_SNOW = vec3(0.86, 0.89, 0.94);
const float BROAD_FREQUENCY = 0.035;
const vec3 PACKED_SNOW_TINT = vec3(0.8, 0.86, 0.95);
const vec3 BAKED_SAND_TINT = vec3(0.9, 0.85, 0.78);

const float LINE_HALF_PIXELS = 0.5;
const float HALF_LEVEL = 0.5;
const float CONTOUR_DEPTH = 0.75;
const float INDEX_CONTOUR_EVERY = 5.0;
const float INDEX_CONTOUR_WEIGHT = 1.6;
const float CROWDED_CONTOUR_PIXELS = 3.0;
const float SPACED_CONTOUR_PIXELS = 9.0;
const float MIN_RELIEF_SPAN = 6.0;
const vec3 UPRIGHT = vec3(0.0, 0.0, 1.0);
const vec3 LOWLAND = vec3(0.90, 0.95, 0.86);
const vec3 HIGHLAND = vec3(1.06, 1.04, 0.98);
const float TERRAIN_ROUGHNESS = 0.85;
const float TOY_ROUGHNESS = 0.72;
const float SNOW_ROUGHNESS = 0.7;
const float SAND_ROUGHNESS = 0.9;
const float WALL_MARK = 0.75;
const float RETAINING_MARK = 0.9375;
const float EDGE_MARK = 0.375;
const float FILL_MARK = 0.25;
const vec2 CHANNEL_WALL_LEVELS = vec2(0.42, 0.52);
const float SUBMERGED_MARK = 0.08;
const float RIVERBED_DEEP_MARK = 0.2;
const float DROWNED_LEVELS = 0.6;
const vec2 DROWNED_SHEER = vec2(1.6, 2.4);
const float SCATTERED_SHADOW = 0.7;
const float SCATTERING_LEVELS = 0.8;

const float BLEND_WIDTH = 0.28;
const float BUILT_BLEND_WIDTH = 0.03;
const float BLEND_PIXELS = 0.8;
const float BLOCKS_BLUR_PIXELS = 4.0;
const float BLOCKS_SHOWN_PIXELS = 12.0;
const float NATURAL_WARP = 0.32;
const float CLIMATE_WARP = 0.6;
const float CLIMATE_SWAY = 1.1;
const float CLIMATE_SWAY_FREQUENCY = 0.19;
const float CLIMATE_FRAY = 1.0;
const float CLIMATE_FRAY_FREQUENCY = 0.33;
const vec2 MAPPED_LUSH_EDGES = vec2(0.15, 0.85);
const float MAPPED_LUSH_RAGGED = 0.6;
const vec2 TRODDEN_EDGES = vec2(0.05, 0.4);
const float TRODDEN_RAGGED = 0.4;
const float BUILT_WARP = 0.06;
const float FIELD_WARP = 0.18;
const float FIELD_BLEND_WIDTH = 0.05;
const float DEPTH_LOD = 3.0;
const int DRY_LEVEL = 3;
const int DRY_REACH = 2;
const float SANDED_SEA = 0.26;
const vec2 SHORE_DAMP = vec2(0.18, 0.28);
const vec2 BANK_DAMP = vec2(0.34, 0.14);
const float DAMP_LEVEL = 0.5;
const float BANK_SWAY = 0.3;
const vec2 BANK_GRAVEL_LEVELS = vec2(0.42, 0.54);
const float BANK_GRAVEL_RAGGED = 0.12;
const float WET_GRAVEL_SHADE = 0.8;

const vec3 SOIL = vec3(0.45, 0.37, 0.27);
const vec3 TOY_SOIL = vec3(0.86, 0.66, 0.55);
const vec3 TOY_MINT = vec3(0.5, 0.86, 0.48);
const float TOY_LAWN_CONTRAST = 0.9;
const float TOY_LAWN_PLOT_TILES = 3.0;
const float MOWN_STRIPES_PER_TILE = 4.0;
const float MOWN_SHADE = 0.06;
const vec2 MOWN_EDGES = vec2(0.35, 0.65);
const float ROUGH_BARE = 0.6;
const float TOY_ROUGH_BARE = 0.2;
const vec3 LUSH_DARK = vec3(0.2, 0.35, 0.17);
const vec3 LUSH_LIGHT = vec3(0.31, 0.47, 0.23);
const vec3 PARCHED_DARK = vec3(0.40, 0.44, 0.27);
const vec3 PARCHED_LIGHT = vec3(0.66, 0.58, 0.34);
const vec2 BROAD_SPAN = vec2(0.25, 0.75);
const float MOISTURE_FREQUENCY = 0.03;
const float MOISTURE_DRIFT_FREQUENCY = 0.09;
const float WATERSIDE_LOD = 3.0;
const float WATERSIDE_WETTING = 1.5;
const float WATERSIDE_MOST = 0.3;
const float UPLAND_DRYING = 0.07;
const float SUNNY_SLOPE_DRYING = 0.11;
const vec2 SUNNY_SIDE = vec2(0.6, 0.8);
const vec2 LUSH_MOISTURE = vec2(0.46, 0.74);
const vec2 PARCHED_MOISTURE = vec2(0.1, 0.42);
const vec3 ROUGH_TINT = vec3(0.55, 0.50, 0.30);
const vec3 STRAW_TINT = vec3(1.14, 1.04, 0.78);
const float STRAW_PATCHES_PER_TILE = 3.0;
const float STRAW_MEAN = 0.09;
const vec3 DEEP_SWARD = vec3(0.74, 0.88, 0.84);
const vec2 DEEP_SWARD_EDGES = vec2(0.22, 0.48);
const float SWARD_PATCHES_PER_TILE = 0.8;
const vec2 LONG_SWARD_EDGES = vec2(0.3, 0.72);
const float LONG_SWARD_MEAN = 0.45;
const vec2 LONG_SWARD_SEED = vec2(27.1, 3.9);
const vec3 LONG_SWARD = vec3(0.92, 0.9, 0.97);
const vec3 GRAZED_SWARD = vec3(1.06, 1.08, 1.02);
const float GRAZED_RANK = 0.4;
const float SWARD_BLADES = 0.6;
const float SEED_HEADS = 0.35;
const float HERB_DRIFTS_PER_TILE = 0.6;
const vec2 HERB_DRIFT_EDGES = vec2(0.6, 0.74);
const vec2 HERB_DRIFT_SEED = vec2(41.3, 12.2);
const float HERB_KINDS_PER_TILE = 0.17;
const vec2 HERB_KIND_SEED = vec2(8.9, 30.5);
const vec3 HERBAGE = vec3(0.88, 0.94, 0.88);
const float FLOWERS_PER_TILE = 18.0;
const float FLOWER_SHARE = 0.4;
const float FLOWER_RADIUS = 0.4;
const float FLOWER_COVER = 0.12;
const SpeckLattice FLOWER_LATTICE = SpeckLattice(ivec2(6007, 2333), 0.15, 0.5, 0.4);
const vec3 DAISY = vec3(0.94, 0.93, 0.88);
const vec3 BUTTERCUP = vec3(0.96, 0.85, 0.22);
const vec3 KNAPWEED = vec3(0.7, 0.44, 0.76);
const vec3 ROCK = vec3(0.50, 0.49, 0.47);
const vec3 ROCK_WARM = vec3(0.58, 0.52, 0.44);
const vec3 SCREE = vec3(0.55, 0.52, 0.47);
const vec3 SCOURED_SNOW = vec3(0.76, 0.82, 0.9);
const vec3 RED_SAND = vec3(0.85, 0.64, 0.43);
const vec3 GRAVEL = vec3(0.74, 0.62, 0.48);
const vec3 PACKED_EARTH = vec3(0.72, 0.57, 0.43);
const vec3 DRY_SAND = vec3(0.78, 0.71, 0.55);
const vec3 PAVING = vec3(0.57, 0.55, 0.51);
const vec3 SANDSTONE_PAVING = vec3(0.7, 0.62, 0.5);
const vec3 YARD = vec3(0.49, 0.41, 0.32);
const vec3 BANK = vec3(0.50, 0.50, 0.48);
const vec3 WET_SAND = vec3(0.50, 0.46, 0.37);
const vec3 SILT = vec3(0.38, 0.40, 0.33);
const vec3 MUD = vec3(0.31, 0.29, 0.22);
const vec3 RIVER_GRAVEL = vec3(0.52, 0.47, 0.37);
const vec3 PEBBLES = vec3(0.64, 0.60, 0.52);
const vec2 PEBBLE_EDGES = vec2(0.55, 0.8);
const vec3 CANAL_FLOOR = vec3(0.45, 0.45, 0.42);
const vec3 HEDGE = vec3(0.24, 0.33, 0.16);

const float STRATA_PER_LEVEL = 2.5;
const float TOY_STRATA_PER_LEVEL = 1.0;
const float STRATA_WARP = 1.8;
const float STRATA_WARP_PER_TILE = 0.3;
const vec2 STRATA_DIP = vec2(0.09, -0.06);
const float STRATA_BOW = 0.35;
const float STRATA_BOWS_PER_TILE = 1.1;
const float STRATA_BOW_SEED = 5.3;
const float BED_THICKNESS_PER_LEVEL = 0.7;
const float BED_THICKNESS_SEED = 3.1;
const vec2 SHELF_EDGES = vec2(0.7, 0.95);
const float LAYER_VARIETY = 0.3;
const float LEDGE_SHADE = 0.8;
const float LEDGE_LIT = 1.08;
const float LEDGE_RISE = 0.2;
const float LEDGE_RUNS_PER_TILE = 1.6;
const vec2 LEDGE_RUN_EDGES = vec2(0.4, 0.6);
const float LEDGE_RUN_SHIFT = 40.0;
const int LEDGE_RUN_SEED = 23;
const float SLABS_PER_TILE = 1.7;
const float SLAB_VARIETY = 0.3;
const float SLAB_SEED = 2.0;
const float SLAB_WARMTH = 0.3;
const vec2 ROCK_GRAIN_VARIETY = vec2(0.22, 0.3);
const float LEVEL_BEDDING = 0.35;
const vec3 ROCK_WEATHERING = vec3(0.72, 0.74, 0.66);
const float ROCK_WEATHERING_PER_TILE = 2.7;
const vec2 ROCK_WEATHERING_EDGES = vec2(0.55, 0.8);
const float ROCK_WEATHERING_SEED = 4.4;
const float CRACKS_PER_TILE = 3.1;
const float CRACK_WIDTH = 0.05;
const float CRACK_DEPTH = 0.18;
const float OUTCROPS_PER_TILE = 0.9;
const float STONE_PATCHES_PER_TILE = 1.7;
const float STONE_GAPS = 0.3;
const float PALE_STONE = 1.15;
const float BOULDERS_PER_TILE = 5.5;
const float GULLIES_PER_TILE = 2.8;
const float GULLY_WANDER = 0.7;
const float GULLY_WANDER_PER_TILE = 0.8;
const float GULLY_RUNS_PER_TILE = 0.9;
const vec2 GULLY_RUN_EDGES = vec2(0.35, 0.6);
const vec2 GULLY_SLOPES = vec2(0.45, 0.95);
const vec2 GULLY_WIDTHS = vec2(0.2, 0.4);
const vec2 GULLY_DEPTHS = vec2(0.15, 0.4);
const float GULLY_RIB = 0.1;
const float GULLY_SEED = 13.7;
const float SCREE_DRIFTS_PER_TILE = 1.3;
const vec2 SCREE_DRIFT_SEED = vec2(2.6, 19.4);
const float SCREE_SPARSE = 0.25;
const vec2 ARMOUR_REACH = vec2(0.3, 0.7);
const float ARMOUR_RAGS_PER_TILE = 2.3;
const float ARMOUR_RAGGED = 0.4;
const float MICRO_PER_TILE = 17.0;
const float CLUMPS_PER_TILE = 3.3;
const float LOCAL_PER_TILE = 1.3;
const float RELIEF_DEPTH = 0.035;
const float STONY_HOLLOWS = 1.0;
const float GRAIN_MEAN = 0.5;
const vec2 BUMP_HEIGHT_SHARES = vec2(0.65, 0.35);
const float BUMP_MEAN_HEIGHT = dot(BUMP_HEIGHT_SHARES, vec2(GRAIN_MEAN));
const vec2 CAVITY_OCCLUSION = vec2(1.6, 0.9);

const vec2 GRASS_RUGGED = vec2(0.45, 0.0);
const vec2 ROUGH_RUGGED = vec2(0.7, 0.25);
const vec2 SCREE_RUGGED = vec2(0.6, 1.0);
const vec2 ROCK_RUGGED = vec2(1.0, 0.55);
const vec2 FIELD_RUGGED = vec2(0.35, 0.05);
const vec2 SAND_RUGGED = vec2(0.2, 0.05);
const vec2 PAVED_RUGGED = vec2(0.1, 0.0);
const vec2 SNOW_RUGGED = vec2(0.12, 0.0);

const float TAU = 6.2831853;
const vec2 WIND = vec2(0.8, 0.6);
const vec2 ACROSS_WIND = vec2(-0.6, 0.8);
const float DRIFT_ALONG = 0.22;
const float DRIFT_ACROSS = 0.8;
const float DRIFT_HEIGHT = 0.3;
const float RIDGE_ALONG = 0.45;
const float RIDGE_ACROSS = 1.4;
const float RIDGE_HEIGHT = 0.07;
const float SASTRUGI_ALONG = 1.6;
const float SASTRUGI_ACROSS = 6.0;
const float SASTRUGI_HEIGHT = 0.012;
const float GLINT_PIXELS = 3.0;
const int GLINT_LEVEL_STRIDE = 7919;
const float GLINT_SHARE = 0.2;
const float GLINT_SPREAD = 3.0;
const float GLINT_FOCUS = 80.0;
const float GLINT_GAIN = 7.0;
const float GLINT_FROM_PIXELS = 10.0;
const float GLINT_TO_PIXELS = 40.0;
const float DUNE_SPACING = 3.4;
const float DUNE_CREST = 0.65;
const float DUNE_HEIGHT = 0.18;
const float DUNE_WARP = 1.3;
const float DUNE_WARP_FREQUENCY = 0.11;
const float DUNE_SWELL_FREQUENCY = 0.07;
const float DUNE_CALM = 0.22;
const float DUNE_STRETCH = 0.9;
const float DUNE_STRETCH_FREQUENCY = 0.045;
const vec2 CROSS_WIND = vec2(0.47, 0.88);
const float CROSS_SPACING = 1.7;
const float CROSS_SHARE_LOW = 0.15;
const float CROSS_SHARE_HIGH = 1.4;
const float CROSS_SHARE_FREQUENCY = 0.035;
const float RIPPLES_PER_TILE = 9.0;
const float RIPPLE_WARP = 0.5;
const float RIPPLE_HEIGHT = 0.0025;
const float RIPPLE_SHADE = 0.05;
const float CROSS_RIPPLES_PER_TILE = 6.3;
const float RIPPLE_BLEND_FREQUENCY = 0.08;
const float TOY_GRAIN = 0.45;
const float TOY_PEBBLE_DRIFTS_PER_TILE = 2.4;
const vec2 TOY_PEBBLE_SEED = vec2(8.3, 4.1);
const vec2 TOY_PEBBLE_BLEND = vec2(0.2, 0.8);

const int TOY_STRATA_COUNT = 4;
const vec3 TOY_STRATA[TOY_STRATA_COUNT] = vec3[TOY_STRATA_COUNT](
	vec3(0.95, 0.66, 0.76),
	vec3(0.76, 0.69, 0.93),
	vec3(0.99, 0.82, 0.6),
	vec3(0.62, 0.86, 0.9)
);

const int CROP_COUNT = 6;
const vec3 CROPS[CROP_COUNT] = vec3[CROP_COUNT](
	vec3(0.52, 0.40, 0.27),
	vec3(0.47, 0.60, 0.27),
	vec3(0.79, 0.68, 0.33),
	vec3(0.85, 0.79, 0.32),
	vec3(0.64, 0.56, 0.30),
	vec3(0.40, 0.53, 0.30)
);
const float FURROWS_PER_TILE = 7.0;
const float FURROW_DEPTH = 0.55;
const float LEVEL_FIELD_SLOPE = 0.25;
const float HILLSIDE_FIELD_SLOPE = 0.6;
const float TRAMLINES_PER_TILE = 1.0;
const float TRAMLINE_GAUGE = 0.045;
const float TRAMLINE_HALF_WIDTH = 0.012;
const float TRAMLINE_DEPTH = 0.6;
const float HEDGE_WIDTH = 0.05;
const float HEDGE_FRINGE = 2.5;
const float HEDGE_FRINGE_SHADE = 0.25;
const float HEDGE_GAPS_PER_TILE = 1.9;
const float HEDGE_OPACITY = 0.85;
const vec3 VERGE = vec3(0.40, 0.44, 0.22);
const float VERGE_WIDTH = 0.08;
const float VERGE_OPACITY = 0.8;
const float CROP_FLOW = 0.32;
const float RIPENING_PER_TILE = 0.9;
const vec3 UNRIPE = vec3(0.84, 1.02, 0.74);
const vec2 UNRIPE_EDGES = vec2(0.5, 0.75);
const float RIPENING_SEED = 3.7;

const float FOREST_COVER = 0.9;
const float CROWN_SHADE = 0.7;
const float CANOPY_OCCLUSION = 0.45;
const float CANOPY_VARIETY = 0.2;
const vec2 LITTER_DENSITY = vec2(0.3, 0.8);
const vec2 LITTER_PATCH_EDGES = vec2(0.3, 0.7);
const float LITTER_PATCHINESS = 0.45;
const float LITTER_VARIETY = 0.2;
const vec3 LITTER_MOSS = vec3(0.29, 0.35, 0.17);
const float MOSS_PATCHES_PER_TILE = 1.6;
const vec2 MOSS_EDGES = vec2(0.55, 0.8);
const float MOSS_SEED = 4.3;
const vec2 LITTER_SHOWN_PIXELS = vec2(8.0, 24.0);
const float FAR_FLOOR_SHADE = 0.7;
const float GRID_DEPTH = 0.6;
const float HIDDEN_BAND_MARGIN = 1.1;

const float COURSES_PER_TILE = 12.0;
const float BLOCKS_PER_TILE = 6.0;
const float MORTAR_SHARE = 0.08;
const vec3 MASONRY = vec3(0.56, 0.53, 0.49);
const float COPING_DEPTH = 0.035;
const float COPING_LIFT = 1.15;
const float DRIP_WIDTH = 0.012;
const float DRIP_SHADE = 0.45;
const float STREAKS_PER_TILE = 7.0;
const float STREAK_LENGTHS_PER_TILE = 3.5;
const vec2 STREAK_EDGES = vec2(0.5, 0.8);
const vec2 STREAK_REACHES = vec2(0.15, 0.5);
const float STREAK_SHADE = 0.7;
const vec2 STREAK_SEEDS = vec2(2.3, 9.1);
const float DAMP_REACH = 0.1;
const float DAMP_RAGS_PER_TILE = 3.0;
const float DAMP_RAGGED = 0.8;
const float DAMP_SEED = 3.7;
const vec3 DAMP_STONE = vec3(0.62, 0.68, 0.52);

const int EARTH_STRATA_COUNT = 5;
const vec3 EARTH_STRATA[EARTH_STRATA_COUNT] = vec3[EARTH_STRATA_COUNT](
	vec3(0.55, 0.42, 0.29),
	vec3(0.70, 0.57, 0.40),
	vec3(0.62, 0.55, 0.46),
	vec3(0.50, 0.47, 0.43),
	vec3(0.64, 0.47, 0.33)
);
const float EDGE_STRATA_PER_TILE = 1.6;
const float EDGE_STRATA_WARP = 0.6;
const float EDGE_STRATA_SWAY = 0.15;
const float EDGE_LAYER_VARIETY = 0.12;
const float EDGE_GRAIN_PER_TILE = 4.0;
const float EDGE_SEAM_SHADE = 0.82;
const float EDGE_WET_SHADE = 0.6;
const float EDGE_WET_REACH = 0.35;
const float EDGE_TURF = 0.12;
const float EDGE_TOPSOIL = 0.35;
const float EDGE_TOPSOIL_SHADE = 0.9;
const float EDGE_TOPSOIL_SWAY = 0.7;
const float EDGE_TOPSOIL_SWING = 0.6;
const float EDGE_ROOTS_PER_TILE = 9.0;
const float EDGE_ROOTS_RAGGED = 0.6;

const vec3 RUNWAY_ASPHALT = vec3(0.25, 0.26, 0.27);
const vec3 APRON_CONCRETE = vec3(0.66, 0.65, 0.62);
const vec3 AIRFIELD_WHITE = vec3(0.92, 0.92, 0.9);
const vec3 TAXI_YELLOW = vec3(0.9, 0.72, 0.18);
const vec3 EDGE_LIGHT = vec3(1.0, 0.93, 0.7);
const float RUNWAY_EDGE = 0.41;
const float RUNWAY_EDGE_HALF = 0.015;
const float CENTRELINE_HALF = 0.012;
const float CENTRELINE_DASHES = 3.0;
const float THRESHOLD_FROM = 0.72;
const float THRESHOLD_TO = 0.9;
const float THRESHOLD_REACH = 0.36;
const float THRESHOLD_BARS = 10.0;
const float EDGE_LIGHT_LATERAL = 0.46;
const float EDGE_LIGHTS = 4.0;
const float EDGE_LIGHT_RADIUS = 0.018;
const float TAXI_LINE_HALF = 0.014;
const float PANEL_JOINTS = 4.0;
const float PANEL_JOINT_HALF = 0.006;
const float STAND_BOX = 0.3;
const float HELIPAD_RING = 0.32;
const float HELIPAD_LINE_HALF = 0.02;

struct Ground {
	uint material;
	float density;
	uint variant;
};

struct Grain {
	float broad;
	float local;
	float clump;
	float fine;
	float micro;
	float stones;
	float hollow;
};

/* How the ground leans, the way it faces and how well its rock layers resolve, read before the fragment branches. */
struct Relief {
	float slope;
	float steep;
	float strata;
	vec2 facing;
};

struct Exposure {
	float scree;
	float rock;
};

/* The loose and the solid stone this point would show where its ground wears through, and how far it lies on the shelf atop a layer of the rock. */
struct Bare {
	vec3 scree;
	vec3 rock;
	float shelf;
};

const Bare NO_BARE = Bare(vec3(0.0), vec3(0.0), 0.0);

/* A ground's colour and how much its fine bumps and its stones stand out of it. */
struct Patch {
	vec3 tone;
	vec2 rugged;
};

const Patch NO_PATCH = Patch(vec3(0.0), vec2(0.0));

struct Site {
	vec2 p;
	Grain grain;
	Relief relief;
	Exposure exposure;
	Bare bare;
	float armoured;
	Climate climate;
	float ragged;
	float trodden;
	vec3 blades;
};

/* How the tiles' grounds meet about a point, the noise bending their seams bending the climate's edges as well. */
struct Seams {
	vec2 bend;
	vec2 warp;
	float width;
	float settled;
};

const uint LIE_DRY = 0u;
const uint LIE_BEACH = 1u;
const uint LIE_SUNKEN = 2u;

struct Footing {
	Relief relief;
	float armoured;
	uint lie;
};

struct Blanket {
	float cover;
	vec3 tone;
	vec2 slope;
	vec2 rugged;
	float roughness;
	float glint;
};

const Blanket NO_BLANKET = Blanket(0.0, vec3(0.0), vec2(0.0), vec2(0.0), 0.0, 0.0);

struct Dune {
	float height;
	vec2 slope;
	float along;
	vec2 heading;
	float lee;
};

struct Shade {
	vec3 albedo;
	vec3 normal;
	float roughness;
	float occlusion;
	float glint;
};

struct Water {
	float level;
	float cover;
	float depth;
	vec3 share;
	float sea;
	float canal;
};

struct Canopy {
	vec4 paint;
	float density;
	float shade;
};

struct ForestFloor {
	float cover;
	vec4 litter;
};

/* The ground one screen pixel steps over across and down the screen; lines on the ground fade by how far apart it puts them. */
mat2 footprint;

Grain Moulded(Grain grain)
{
	return Grain(grain.broad, mix(0.5, grain.local, TOY_GRAIN), mix(0.5, grain.clump, TOY_GRAIN), mix(0.5, grain.fine, TOY_GRAIN), mix(0.5, grain.micro, TOY_GRAIN), grain.stones, grain.hollow * TOY_GRAIN);
}

Grain GrainAt(vec2 p, Detail detail)
{
	float local = (2.0 * FacingOctave(detail, LOCAL_PER_TILE, 0.0) + FacingOctave(detail, LOCAL_PER_TILE * 2.07, 17.3)) / 3.0;
	Grain grain = Grain(Layered(p, 0.15), local, FacingOctave(detail, CLUMPS_PER_TILE, 2.2), detail.fine, detail.micro, detail.stones, detail.hollow);
	return Landscape() == LANDSCAPE_TOYLAND ? Moulded(grain) : grain;
}

Ground GroundAt(ivec2 tile)
{
	uvec4 codes = CodesAt(tile);
	return Ground(codes.r, float(codes.g & DENSITY_MASK) / float(DENSITY_MASK), codes.a);
}

float GroundRoughness()
{
	return Landscape() == LANDSCAPE_TOYLAND ? TOY_ROUGHNESS : TERRAIN_ROUGHNESS;
}

vec3 Paving()
{
	return Landscape() == LANDSCAPE_TROPIC ? SANDSTONE_PAVING : PAVING;
}

vec3 Soil()
{
	return Landscape() == LANDSCAPE_TOYLAND ? TOY_SOIL : SOIL;
}

/* Below the sea the waves scatter the sunlight, so the shadows falling on the bed lighten with the depth of the water over it. */
vec4 Lit(Shade shade)
{
	vec3 position = RenderPoint(v_world);
	float scattered = SCATTERED_SHADOW * smoothstep(0.0, SCATTERING_LEVELS, -v_world.z);
	float sunlit = mix(SunVisibility(position, shade.normal), 1.0, scattered);
	return SunlitRadiance(Linear(shade.albedo), shade.normal, ToEye(position), Coat(shade.roughness, DIELECTRIC_REFLECTANCE), shade.occlusion, shade.glint * GLINT_GAIN, sunlit);
}

/* How steeply the ground leans at the fragment, in levels per tile. */
float SlopeOf(vec3 normal)
{
	return length(normal.xy) / max(normal.z, LEAST_DIVISOR) / LevelRise();
}

float StrataPerLevel()
{
	return Landscape() == LANDSCAPE_TOYLAND ? TOY_STRATA_PER_LEVEL : STRATA_PER_LEVEL;
}

float BeddingAt(vec2 p)
{
	float levelled = v_world.z * StrataPerLevel();
	if (Landscape() == LANDSCAPE_TOYLAND) return levelled;
	float thickness = Noise(vec2(v_world.z * BED_THICKNESS_PER_LEVEL, BED_THICKNESS_SEED)) * StrataPerLevel();
	float warp = (Noise(p * STRATA_WARP_PER_TILE) - 0.5) * STRATA_WARP;
	float bow = (Octave(p + STRATA_BOW_SEED, STRATA_BOWS_PER_TILE) - 0.5) * STRATA_BOW;
	return levelled + dot(p, STRATA_DIP) + thickness + warp + bow;
}

Relief ReliefAt(vec3 normal, float levels_per_pixel)
{
	float slope = SlopeOf(normal);
	float layer_pixels = 1.0 / (levels_per_pixel * StrataPerLevel());
	return Relief(slope, smoothstep(STANDARD_SLOPE, STEEPEST_SLOPE, slope), smoothstep(UNRESOLVED_REPEAT_PIXELS, RESOLVED_REPEAT_PIXELS * 2.0, layer_pixels), normal.xy);
}

float AltitudeShare()
{
	return clamp(v_world.z / max(Peak(), MIN_RELIEF_SPAN), 0.0, 1.0);
}

vec3 Altitude(vec3 colour)
{
	float t = AltitudeShare();
	vec3 tint = t < 0.5 ? mix(LOWLAND, vec3(1.0), t * 2.0) : mix(vec3(1.0), HIGHLAND, t * 2.0 - 1.0);
	return colour * tint;
}

float Contour(float height, float levels_per_pixel)
{
	bool index = mod(floor(height) + 1.0, INDEX_CONTOUR_EVERY) == 0.0;
	float weight = index ? INDEX_CONTOUR_WEIGHT : 1.0;
	float line = PixelLine(abs(fract(height) - HALF_LEVEL), levels_per_pixel, LINE_HALF_PIXELS * weight);
	return line * smoothstep(CROWDED_CONTOUR_PIXELS, SPACED_CONTOUR_PIXELS, 1.0 / levels_per_pixel) * weight;
}

/* Toyland's lawns drift between teal, mint and lime, held toward the mint between so they wash gently from one to the next. */
vec3 ToyLawn(float shade)
{
	float calm = mix(0.5, shade, TOY_LAWN_CONTRAST);
	return mix(mix(GRASS_DARK, TOY_MINT, smoothstep(0.2, 0.45, calm)), GRASS_LIGHT, smoothstep(0.55, 0.85, calm));
}

/* Grass shades from a darker to a lighter green across the land, deep green where it grows lush and hay gold where it is parched. */
vec3 GrassTone(float shade, Climate climate)
{
	if (Landscape() == LANDSCAPE_TOYLAND) return ToyLawn(shade);
	float tone = smoothstep(BROAD_SPAN.x, BROAD_SPAN.y, shade);
	vec3 green = mix(mix(GRASS_DARK, GRASS_LIGHT, tone), mix(LUSH_DARK, LUSH_LIGHT, tone), climate.lush);
	return mix(green, mix(PARCHED_DARK, PARCHED_LIGHT, tone), climate.parched);
}

float Hollowed(Grain grain, float depth)
{
	return clamp(1.0 - depth * grain.hollow, 0.55, 1.15);
}

float Cover(float density, float clump)
{
	return density >= 1.0 ? 1.0 : smoothstep(-0.06, 0.06, density + clump - 1.0);
}

float Mown(vec2 p)
{
	if (Landscape() != LANDSCAPE_TOYLAND) return 1.0;
	float shown = Resolved(MOWN_STRIPES_PER_TILE);
	if (shown <= 0.0) return 1.0;
	bool along_x = Hash(ivec2(floor(p / TOY_LAWN_PLOT_TILES))) < 0.5;
	float stripe = abs(fract((along_x ? p.y : p.x) * MOWN_STRIPES_PER_TILE * 0.5) - 0.5) * 2.0;
	return 1.0 + MOWN_SHADE * shown * (2.0 * smoothstep(MOWN_EDGES.x, MOWN_EDGES.y, stripe) - 1.0);
}

float PatchShare(vec2 p, float frequency, vec2 edges, float mean)
{
	float shown = Resolved(frequency);
	return shown > 0.0 ? mix(mean, smoothstep(edges.x, edges.y, Noise(OctaveTurn(frequency) * p * frequency)), shown) : mean;
}

vec3 HerbBloom(float kind)
{
	return mix(mix(DAISY, BUTTERCUP, smoothstep(0.38, 0.46, kind)), KNAPWEED, smoothstep(0.62, 0.7, kind));
}

vec3 Bloomed(vec3 blades, vec2 p)
{
	float drift = PatchShare(p + HERB_DRIFT_SEED, HERB_DRIFTS_PER_TILE, HERB_DRIFT_EDGES, 0.0);
	if (drift <= 0.0) return blades;
	float share = FLOWER_SHARE * drift;
	float shown = Resolved(FLOWERS_PER_TILE);
	float speck = shown > 0.0 ? Speck(OctaveTurn(FLOWERS_PER_TILE) * p * FLOWERS_PER_TILE, share, vec2(1.0, 0.0), vec2(FLOWER_RADIUS), FLOWER_LATTICE) : 0.0;
	float flowers = mix(FLOWER_COVER * share, speck, shown);
	return mix(blades * mix(vec3(1.0), HERBAGE, drift), HerbBloom(Octave(p + HERB_KIND_SEED, HERB_KINDS_PER_TILE)), flowers);
}

/* Grass in long and grazed patches, turned to straw here and there and flecked by its blades and seed heads. */
vec3 Blades(Site site)
{
	Grain grain = site.grain;
	vec3 green = GrassTone(grain.broad, site.climate);
	vec3 straw = green * STRAW_TINT;
	float grown = PatchShare(site.p + LONG_SWARD_SEED, SWARD_PATCHES_PER_TILE, LONG_SWARD_EDGES, LONG_SWARD_MEAN);
	float rank = mix(GRAZED_RANK, 1.0, grown);
	float shade = Varied(grain.local, 0.3) * Varied(grain.clump, 0.35 * rank) * Varied(grain.fine, 0.25) * Varied(grain.micro, SWARD_BLADES * rank);
	float dried = smoothstep(0.5, 0.85, grain.local) + SEED_HEADS * smoothstep(0.6, 0.85, grain.micro);
	float strawed = mix(STRAW_MEAN, dried * rank, Resolved(STRAW_PATCHES_PER_TILE));
	vec3 undergrowth = green * mix(vec3(1.0), DEEP_SWARD, rank);
	vec3 sward = mix(undergrowth, green, smoothstep(DEEP_SWARD_EDGES.x, DEEP_SWARD_EDGES.y, grain.local)) * mix(GRAZED_SWARD, LONG_SWARD, grown);
	return Bloomed(mix(sward, straw, strawed) * shade * Mown(site.p), site.p);
}

vec3 Grass(float density, Site site)
{
	Grain grain = site.grain;
	vec3 soil = Soil() * Varied(grain.local, 0.2) * Varied(grain.micro, 0.25) * mix(1.0, 1.25, grain.stones);
	return mix(soil, site.blades, Cover(density, grain.local));
}

vec3 Meadow(Site site)
{
	return Grass(1.0, site);
}

/* Toyland's strata blended as they read from too far to make out one from another. */
vec3 ToyStrataMean()
{
	return (TOY_STRATA[0] + TOY_STRATA[1] + TOY_STRATA[2] + TOY_STRATA[3]) / float(TOY_STRATA_COUNT);
}

vec3 ToyStratum(int layer)
{
	return TOY_STRATA[abs(layer) % TOY_STRATA_COUNT];
}

vec3 Stratum(int layer)
{
	if (Landscape() == LANDSCAPE_TOYLAND) return ToyStratum(layer);
	return mix(ROCK, ROCK_WARM, Hash(ivec2(layer, 7))) * Varied(Hash(ivec2(layer, 19)), LAYER_VARIETY);
}

vec3 ToyPebbles(vec2 p)
{
	float drift = Octave(p + TOY_PEBBLE_SEED, TOY_PEBBLE_DRIFTS_PER_TILE) * float(TOY_STRATA_COUNT);
	vec3 pebbles = mix(ToyStratum(int(drift)), ToyStratum(int(drift) + 1), smoothstep(TOY_PEBBLE_BLEND.x, TOY_PEBBLE_BLEND.y, fract(drift)));
	return mix(ToyStrataMean(), pebbles, Resolved(TOY_PEBBLE_DRIFTS_PER_TILE));
}

vec3 LooseStone(vec2 p)
{
	return Landscape() == LANDSCAPE_TOYLAND ? ToyPebbles(p) : SCREE;
}

/* Loose stones shed from rock: gravel speckled light and dark, with pebbles strewn over it and dark gaps between them. */
vec3 Scree(Grain grain, vec2 p)
{
	vec3 tone = LooseStone(p) * Varied(grain.local, 0.18) * Varied(grain.fine, 0.14) * Varied(grain.micro, 0.45);
	float strewn = grain.stones * mix(SCREE_SPARSE, 1.0, Octave(p + SCREE_DRIFT_SEED, SCREE_DRIFTS_PER_TILE));
	return tone * mix(1.0, 1.18, strewn) * Hollowed(grain, STONY_HOLLOWS);
}

float Channels(float down, float across)
{
	float wandering = across * GULLIES_PER_TILE + (Noise(vec2(down * GULLY_WANDER_PER_TILE, across * GULLY_WANDER_PER_TILE) + GULLY_SEED) - 0.5) * GULLY_WANDER;
	float channel = floor(wandering);
	vec2 dice = Hash2(ivec2(int(channel), int(GULLY_SEED)));
	float vein = abs(fract(wandering) - 0.5);
	float runs = smoothstep(GULLY_RUN_EDGES.x, GULLY_RUN_EDGES.y, Noise(vec2(down * GULLY_RUNS_PER_TILE, channel + GULLY_SEED)));
	float cut = 1.0 - smoothstep(0.0, mix(GULLY_WIDTHS.x, GULLY_WIDTHS.y, dice.x), 0.5 - vein);
	return (GULLY_RIB * (1.0 - cut) - mix(GULLY_DEPTHS.x, GULLY_DEPTHS.y, dice.y) * cut) * runs;
}

float Gullied(vec2 p, Relief relief)
{
	float lean = length(relief.facing);
	float shown = Resolved(GULLIES_PER_TILE * 2.0) * smoothstep(GULLY_SLOPES.x, GULLY_SLOPES.y, relief.slope);
	if (shown <= 0.0 || lean <= LEAST_DIVISOR) return 1.0;
	vec2 weights = relief.facing * relief.facing / (lean * lean);
	return 1.0 + (Channels(p.x, p.y) * weights.x + Channels(p.y, p.x) * weights.y) * shown;
}

/* Cracks wander through rock, here and there breaking off. */
float Cracks(vec2 p)
{
	float shown = Resolved(CRACKS_PER_TILE * 4.0);
	if (shown <= 0.0) return 0.0;
	float vein = abs(Noise(p * CRACKS_PER_TILE) - 0.5);
	return (1.0 - smoothstep(0.0, CRACK_WIDTH, vein)) * smoothstep(0.4, 0.62, Noise(p * 1.9 + 8.1)) * shown;
}

float LedgeRun(vec2 p, float bedding)
{
	vec2 shift = Hash2(ivec2(int(floor(bedding)), LEDGE_RUN_SEED)) * LEDGE_RUN_SHIFT;
	return smoothstep(LEDGE_RUN_EDGES.x, LEDGE_RUN_EDGES.y, Noise(p * LEDGE_RUNS_PER_TILE + shift));
}

vec3 Rock(vec2 p, float bedding, float ledges, Relief relief, Grain grain)
{
	vec3 bed = Stratum(int(floor(bedding)));
	float ledge = mix(LEDGE_SHADE, LEDGE_LIT, smoothstep(0.0, LEDGE_RISE, fract(bedding)));
	if (Landscape() == LANDSCAPE_TOYLAND) return mix(ToyStrataMean(), bed * ledge, ledges);
	float slab = Varied(Octave(p + SLAB_SEED, SLABS_PER_TILE), SLAB_VARIETY);
	vec3 slabs = mix(ROCK, ROCK_WARM, SLAB_WARMTH) * slab;
	vec3 rock = mix(mix(slabs, bed, LEVEL_BEDDING), bed * ledge * slab, ledges);
	vec3 weathered = mix(rock, rock * ROCK_WEATHERING, smoothstep(ROCK_WEATHERING_EDGES.x, ROCK_WEATHERING_EDGES.y, Octave(p + ROCK_WEATHERING_SEED, ROCK_WEATHERING_PER_TILE)));
	return weathered * Varied(grain.fine, ROCK_GRAIN_VARIETY.x) * Varied(grain.micro, ROCK_GRAIN_VARIETY.y) * (1.0 - CRACK_DEPTH * (1.0 - relief.steep) * Cracks(p));
}

Bare BareAt(vec2 p, Relief relief, Grain grain, bool rocky)
{
	float bedding = BeddingAt(p);
	float standing = relief.steep * relief.strata;
	float runs = Landscape() == LANDSCAPE_TOYLAND || standing <= 0.0 ? 1.0 : LedgeRun(p, bedding);
	float ledges = standing * runs;
	float gullied = Gullied(p, relief);
	vec3 rock = rocky ? Rock(p, bedding, ledges, relief, grain) * gullied : vec3(0.0);
	return Bare(Scree(grain, p) * gullied, rock, smoothstep(SHELF_EDGES.x, SHELF_EDGES.y, fract(bedding)) * ledges);
}

/* Ground lies bare where it stands steep, more so where snow or sand slides off it, or high in the mountains, first as loose scree and then as rock, with edges ragged at every scale. */
Exposure ExposureAt(Relief relief, Grain grain, float blanket)
{
	float altitude = AltitudeShare();
	float ragged = (grain.local - 0.5) * 0.7 + (grain.fine - 0.5) * 0.3 + (grain.micro - 0.5) * 0.15;
	float baring = Landscape() == LANDSCAPE_TOYLAND ? TOY_BARING : 1.0;
	float bare = (relief.steep * (STEEP_BARING + blanket * SHED_BARING) + smoothstep(ALPINE_ALTITUDE, 1.0, altitude) * ALPINE_BARING) * baring + ragged;
	float scree = smoothstep(SCREE_EXPOSURE - EXPOSURE_BLEND, SCREE_EXPOSURE + EXPOSURE_BLEND, bare);
	float rock = smoothstep(ROCK_EXPOSURE - EXPOSURE_BLEND, ROCK_EXPOSURE + EXPOSURE_BLEND, bare);
	return Exposure(scree, rock);
}

/* Above the snow line and in snowfields snow lingers on bare stone where it can lie: on the shelves atop the rock's layers and on the tops of stones. */
float SnowCaps(Site site)
{
	if (Landscape() != LANDSCAPE_ARCTIC) return 0.0;
	float snowy = max(site.climate.blanket, smoothstep(0.0, SNOW_LINE_PULL, SnowLift(v_world.z + site.ragged * SNOW_LINE_RAGGED)));
	float perch = max(site.bare.shelf, smoothstep(0.55, 0.85, site.grain.stones + site.grain.fine * 0.3 - 0.15));
	return snowy * perch * CAP_COVER;
}

vec3 Capped(vec3 stone, Site site)
{
	return mix(stone, CAP_SNOW * Varied(site.grain.micro, 0.08), SnowCaps(site));
}

Patch Exposed(Patch ground, Site site)
{
	vec3 tone = mix(mix(ground.tone, Capped(site.bare.scree, site), site.exposure.scree), Capped(site.bare.rock, site), site.exposure.rock);
	vec2 rugged = mix(mix(ground.rugged, SCREE_RUGGED, site.exposure.scree), ROCK_RUGGED, site.exposure.rock);
	return Patch(tone, rugged);
}

/* Rough land: tussocks of dry and green grass in clumps, opening onto patches of stony soil. */
Patch Rough(Site site)
{
	Grain grain = site.grain;
	vec2 p = site.p;
	vec3 green = GrassTone(grain.broad, site.climate) * 0.9;
	vec3 dry = mix(green, Landscape() == LANDSCAPE_TOYLAND ? GRASS_LIGHT : ROUGH_TINT, 0.5);
	float clumps = smoothstep(0.35, 0.65, Octave(p, 2.3));
	float tussocks = smoothstep(0.4, 0.75, 0.6 * grain.micro + 0.4 * grain.fine);
	vec3 scrub = mix(green, dry, clumps) * mix(1.08, 0.84, tussocks) * Varied(grain.micro, 0.3);
	float open = smoothstep(0.58, 0.8, 0.65 * Octave(p + 3.3, 1.2) + 0.35 * grain.fine);
	vec3 stony = mix(Soil(), LooseStone(p), 0.65) * mix(0.92, 1.15, clamp(grain.stones * 1.6, 0.0, 1.0)) * Varied(grain.micro, 0.3);
	return Patch(mix(scrub, stony, open * (Landscape() == LANDSCAPE_TOYLAND ? TOY_ROUGH_BARE : ROUGH_BARE)), ROUGH_RUGGED);
}

/* Rocky ground: grass with stones gathered in patches over it, and pale rock breaking through here and there, capped with snow where snow lies. */
Patch Rocks(Site site)
{
	Grain grain = site.grain;
	vec2 p = site.p;
	Bare bare = Bare(Capped(site.bare.scree, site), Capped(site.bare.rock, site), site.bare.shelf);
	float patches = smoothstep(0.48, 0.72, 0.7 * Noise(p * STONE_PATCHES_PER_TILE + 6.2) + 0.3 * grain.fine);
	float stones = patches * mix(STONE_GAPS, 1.0, clamp(grain.stones * 1.6, 0.0, 1.0));
	float outcrop = smoothstep(0.64, 0.74, 0.7 * Noise(p * OUTCROPS_PER_TILE + 2.9) + 0.3 * grain.fine);
	vec3 strewn = mix(Meadow(site), bare.scree * PALE_STONE, stones);
	return Patch(mix(strewn, bare.rock * PALE_STONE, outcrop), mix(mix(GRASS_RUGGED, SCREE_RUGGED, patches), ROCK_RUGGED, outcrop));
}

/* A field's crop grows in rows across it, ripening and thinning unevenly, with bare soil between the rows and a pair of wheel tracks every so often;
 * rows stay sharp seen end on and fade only as they crowd closer than the pixels. On a hillside it grows rough without rows, so no lattice is drawn over the slope. */
Patch Fields(Ground ground, Grain grain, vec2 p)
{
	vec3 crop = CROPS[int(ground.variant % uint(CROP_COUNT))] * Varied(Octave(p + 1.9, 0.6), CROP_FLOW) * Varied(Octave(p + 7.4, 2.1), 0.1) * Varied(grain.local, 0.1);
	crop *= mix(vec3(1.0), UNRIPE, smoothstep(UNRIPE_EDGES.x, UNRIPE_EDGES.y, Octave(p + RIPENING_SEED, RIPENING_PER_TILE)));
	bool rows_along_y = (ground.variant & 1u) == 0u;
	float across = rows_along_y ? p.x : p.y;
	float along = rows_along_y ? p.y : p.x;
	float row_pitch = Pitch(footprint, rows_along_y ? vec2(1.0, 0.0) : vec2(0.0, 1.0));
	float level = 1.0 - smoothstep(LEVEL_FIELD_SLOPE, HILLSIDE_FIELD_SLOPE, SlopeOf(normalize(v_normal)));
	float gap = abs(fract(across * FURROWS_PER_TILE) - 0.5) * 2.0;
	float furrow = smoothstep(0.45, 0.95, gap) * level * ResolvedAt(FURROWS_PER_TILE, 1.0 / row_pitch);
	float track_gap = abs(abs(fract(across * TRAMLINES_PER_TILE) - 0.5) / TRAMLINES_PER_TILE - TRAMLINE_GAUGE);
	float track = Stroke(track_gap, TRAMLINE_HALF_WIDTH, row_pitch) * level;
	float stalks = ResolvedAt(MICRO_PER_TILE, 1.0 / row_pitch);
	float plants = (stalks > 0.0 ? Varied(Noise(vec2(across * FURROWS_PER_TILE, along * MICRO_PER_TILE)), 0.3 * stalks) : 1.0) * Varied(grain.micro, 0.25);
	vec3 soil = mix(Soil(), crop, 0.25) * 0.85 * Varied(grain.fine, 0.2);
	return Patch(mix(mix(crop * plants, soil, furrow * FURROW_DEPTH), soil, track * TRAMLINE_DEPTH), FIELD_RUGGED);
}

bool CarriesWay(ivec2 tile)
{
	uvec4 ways = texelFetch(u_network, Clamped(tile), 0);
	return (ways.x | ways.y | ways.z) != 0u;
}

/* A shore is heaped with stone against the sea near a tile carrying a railway or a road, giving way to the beach along a ragged edge. */
float Armoured(vec2 p)
{
	ivec2 tile = ivec2(floor(p));
	float nearest = ARMOUR_REACH.y + ARMOUR_RAGGED;
	for (int dy = -1; dy <= 1; dy++) {
		for (int dx = -1; dx <= 1; dx++) {
			ivec2 near = tile + ivec2(dx, dy);
			if (CarriesWay(near)) nearest = min(nearest, length(max(abs(p - vec2(near) - TILE_CENTRE) - HALF_TILE, 0.0)));
		}
	}
	float ragged = (Noise(p * ARMOUR_RAGS_PER_TILE) - 0.5) * ARMOUR_RAGGED;
	return 1.0 - smoothstep(ARMOUR_REACH.x, ARMOUR_REACH.y, nearest + ragged);
}

Patch Mixed(Patch ground, Patch over, float share)
{
	return Patch(mix(ground.tone, over.tone, share), mix(ground.rugged, over.rugged, share));
}

/* A beach is wet sand at the water's edge drying up the shore, and grass where the shore rises well clear of the sea. */
Patch Beach(Site site)
{
	Grain grain = site.grain;
	float lift = v_world.z + (grain.fine - 0.5) * 0.12 + (grain.micro - 0.5) * 0.05;
	vec3 sand = mix(WET_SAND, DRY_SAND, smoothstep(0.03, 0.22, lift)) * Varied(grain.local, 0.12) * Varied(grain.micro, 0.12);
	float grassed = smoothstep(0.45, 0.75, lift + (grain.local - 0.5) * 0.4);
	return Patch(mix(sand, Meadow(site), grassed), mix(SAND_RUGGED, GRASS_RUGGED, grassed));
}

/* Heaped stone armouring a shore: grey boulders, lit on top and dark in the gaps between. */
vec3 Riprap(vec2 p, Bare bare)
{
	float lumps = smoothstep(0.3, 0.7, Octave(p, BOULDERS_PER_TILE));
	vec3 stone = mix(bare.scree, ROCK, 0.5) * Varied(Octave(p + 7.7, BOULDERS_PER_TILE * 0.45), 0.3);
	return stone * mix(0.62, 1.12, lumps);
}

/* Beside a railway or a road the shore is armoured with heaped stone. */
Patch Shore(Site site)
{
	if (site.armoured <= 0.0) return Beach(site);
	Patch stone = Patch(Riprap(site.p, site.bare), SCREE_RUGGED);
	return site.armoured < 1.0 ? Mixed(Beach(site), stone, site.armoured) : stone;
}

vec3 Steppe(Site site)
{
	return mix(Meadow(site) * STRAW_TINT, DRY_SAND * Varied(site.grain.micro, 0.15), 0.4);
}

Patch Albedo(Ground ground, Site site)
{
	Grain grain = site.grain;
	vec2 p = site.p;
	switch (ground.material) {
		case MAT_GRASS: return Patch(Grass(ground.density, site), GRASS_RUGGED);
		case MAT_ROUGH:
		case MAT_SNOW: return Rough(site);
		case MAT_ROCKS: return Rocks(site);
		case MAT_FIELDS: return Fields(ground, grain, p);
		case MAT_DESERT: return Patch(Steppe(site), SAND_RUGGED);
		case MAT_SHORE: return Shore(site);
		case MAT_PAVED: return Patch(Paving() * (0.94 + 0.08 * grain.local) * Varied(grain.fine, 0.06) * Varied(grain.micro, 0.08), PAVED_RUGGED);
		case MAT_DIRT: return Patch(YARD * Varied(grain.local, 0.15) * Varied(grain.fine, 0.1) * Varied(grain.micro, 0.2), ROUGH_RUGGED);
	}
	return Patch(VOID_TONE, vec2(0.0));
}

bool IsBuilt(uint material)
{
	return material == MAT_PAVED || material == MAT_DIRT;
}

bool IsNatural(uint material)
{
	return material == MAT_GRASS || material == MAT_ROUGH || material == MAT_SNOW || material == MAT_DESERT || material == MAT_SHORE;
}

bool Grassed(uint material)
{
	return material == MAT_GRASS || material == MAT_ROCKS || material == MAT_DESERT || material == MAT_SHORE;
}

Patch Trodden(Patch ground, Site site)
{
	if (site.trodden <= 0.0) return ground;
	vec3 earth = PACKED_EARTH * Varied(site.grain.local, 0.14) * Varied(site.grain.fine, 0.08) * Varied(site.grain.micro, 0.12);
	return Mixed(ground, Patch(earth, PAVED_RUGGED), site.trodden);
}

Patch Eroded(Ground ground, Site site)
{
	Patch patch = Albedo(ground, site);
	return IsNatural(ground.material) ? Trodden(Exposed(patch, site), site) : patch;
}

vec3 SettledAt(ivec2 tile)
{
	uvec4 codes = CodesAt(tile);
	float built = float(IsBuilt(codes.r));
	float tilled = float(codes.r == MAT_FIELDS);
	return vec3(built, tilled, max(max(built, tilled), float((codes.g & BUILT_BIT) != 0u)));
}

vec3 Settlement(vec2 p)
{
	vec2 q = p - TILE_CENTRE;
	ivec2 cell = ivec2(floor(q));
	vec2 f = fract(q);
	return mix(mix(SettledAt(cell), SettledAt(cell + ivec2(1, 0)), f.x), mix(SettledAt(cell + ivec2(0, 1)), SettledAt(cell + ivec2(1, 1)), f.x), f.y);
}

vec2 Bend(vec2 p)
{
	return (vec2(Noise(p * 0.9), Noise(p * 0.9 + 41.7)) - 0.5) * 2.0;
}

/* From too far to make out its blocks, all ground blends as open land does. */
Seams SeamsAt(vec2 p)
{
	vec2 bend = Bend(p);
	vec3 settlement = Settlement(p);
	float shown = smoothstep(BLOCKS_BLUR_PIXELS, BLOCKS_SHOWN_PIXELS, tile_pixels);
	float built = min(settlement.x * 2.0, 1.0) * shown;
	float tilled = min(settlement.y * 2.0, 1.0) * shown;
	float sharpest = BLEND_PIXELS / tile_pixels;
	float reach = mix(mix(NATURAL_WARP, FIELD_WARP, tilled), BUILT_WARP, built);
	float width = mix(mix(BLEND_WIDTH, max(FIELD_BLEND_WIDTH, sharpest), tilled), max(BUILT_BLEND_WIDTH, sharpest), built);
	return Seams(bend, bend * reach, width, settlement.z);
}

bool HasCorner(Ground corners[4], uint material)
{
	return corners[0].material == material || corners[1].material == material || corners[2].material == material || corners[3].material == material;
}

bool ShowsClimate(uint material)
{
	return IsNatural(material) || material == MAT_ROCKS;
}

bool AnyShowsClimate(Ground corners[4])
{
	return ShowsClimate(corners[0].material) || ShowsClimate(corners[1].material) || ShowsClimate(corners[2].material) || ShowsClimate(corners[3].material);
}

float Ragged(vec2 p, Grain grain)
{
	return (Octave(p, 0.17) - 0.5) * 0.5 + (Octave(p, 0.45) - 0.5) * 0.4 + (grain.local - 0.5) * 0.4 + (grain.fine - 0.5) * 0.2;
}

vec2 Drift(vec2 p, float frequency, float reach)
{
	vec2 q = OctaveTurn(frequency) * p * frequency;
	return (vec2(Noise(q + 3.3), Noise(q + 9.1)) - 0.5) * 2.0 * reach;
}

/* The midday sun stands on one side of the map. */
float Moisture(vec2 p, Relief relief)
{
	float drifts = (2.0 * Octave(p + 11.3, MOISTURE_FREQUENCY) + Octave(p + 4.9, MOISTURE_DRIFT_FREQUENCY)) / 3.0;
	float waterside = min(textureLod(u_water, p / MapSize(), WATERSIDE_LOD).r * WATERSIDE_WETTING, WATERSIDE_MOST);
	float upland = AltitudeShare();
	return drifts + waterside - upland * UPLAND_DRYING - dot(relief.facing, SUNNY_SIDE) * SUNNY_SLOPE_DRYING;
}

bool ClimateMapped()
{
	return Landscape() == LANDSCAPE_ARCTIC || Landscape() == LANDSCAPE_TROPIC;
}

Climate MappedClimate(vec2 p, float ragged, vec2 warp)
{
	Climate climate = ClimateAt(p + warp + Drift(p, CLIMATE_SWAY_FREQUENCY, CLIMATE_SWAY) + Drift(p, CLIMATE_FRAY_FREQUENCY, CLIMATE_FRAY));
	climate.lush = smoothstep(MAPPED_LUSH_EDGES.x, MAPPED_LUSH_EDGES.y, climate.lush + ragged * MAPPED_LUSH_RAGGED);
	return climate;
}

Climate WateredClimate(vec2 p, Relief relief)
{
	float moisture = Moisture(p, relief);
	return Climate(0.0, smoothstep(LUSH_MOISTURE.x, LUSH_MOISTURE.y, moisture), 1.0 - smoothstep(PARCHED_MOISTURE.x, PARCHED_MOISTURE.y, moisture));
}

Climate SiteClimate(vec2 p, float ragged, Relief relief, Ground corners[4], vec2 warp)
{
	if (ClimateMapped()) return MappedClimate(p, ragged, warp);
	if (Landscape() == LANDSCAPE_TOYLAND || !AnyShowsClimate(corners)) return Climate(0.0, 0.0, 0.0);
	return WateredClimate(p, relief);
}

Site SiteAt(vec2 p, Seams seams, Grain grain, Footing footing, Ground corners[4])
{
	float ragged = ClimateMapped() ? Ragged(p, grain) : 0.0;
	Climate climate = SiteClimate(p, ragged, footing.relief, corners, seams.bend * CLIMATE_WARP);
	float trodden = Landscape() == LANDSCAPE_TROPIC ? smoothstep(TRODDEN_EDGES.x, TRODDEN_EDGES.y, seams.settled + ragged * TRODDEN_RAGGED) : 0.0;
	Exposure exposure = ExposureAt(footing.relief, grain, climate.blanket);
	bool rocky = exposure.rock > 0.0 || HasCorner(corners, MAT_ROCKS);
	bool bared = footing.lie != LIE_SUNKEN && (exposure.scree > 0.0 || footing.armoured > 0.0 || rocky);
	Bare bare = bared ? BareAt(p, footing.relief, grain, rocky) : NO_BARE;
	return Site(p, grain, footing.relief, exposure, bare, footing.armoured, climate, ragged, trodden, vec3(0.0));
}

/* Each distinct ground is shaded once, in a loop the compiler keeps as one copy. */
Patch Surface(vec2 p, Seams seams, Grain grain, Footing footing, out Site site)
{
	vec2 q = p + seams.warp - TILE_CENTRE;
	ivec2 cell = ivec2(floor(q));
	vec2 f = smoothstep(HALF_TILE - seams.width, HALF_TILE + seams.width, fract(q));

	Ground corners[4] = Ground[4](GroundAt(cell), GroundAt(cell + ivec2(1, 0)), GroundAt(cell + ivec2(0, 1)), GroundAt(cell + ivec2(1, 1)));
	float shares[4] = float[4]((1.0 - f.x) * (1.0 - f.y), f.x * (1.0 - f.y), (1.0 - f.x) * f.y, f.x * f.y);
	site = SiteAt(p, seams, grain, footing, corners);
	if (footing.lie == LIE_SUNKEN) return NO_PATCH;
	Ground distinct[4];
	float weights[4];
	int count = 0;
	for (int i = 0; i < 4; i++) {
		if (shares[i] <= 0.0) continue;
		int k = 0;
		while (k < count && distinct[k] != corners[i]) k++;
		if (k == count) {
			distinct[k] = corners[i];
			weights[k] = 0.0;
			count++;
		}
		weights[k] += shares[i];
	}

	Patch blend = NO_PATCH;
	bool bladed = false;
	for (int k = 0; k < count; k++) {
		if (!bladed && (footing.lie == LIE_BEACH || Grassed(distinct[k].material))) {
			site.blades = Blades(site);
			bladed = true;
		}
		Patch patch = Eroded(distinct[k], site);
		blend.tone += patch.tone * weights[k];
		blend.rugged += patch.rugged * weights[k];
	}
	return blend;
}

bool SameField(ivec2 tile, Ground field)
{
	Ground ground = GroundAt(tile);
	return ground.material == MAT_FIELDS && ground.variant == field.variant;
}

/* A hedge with gaps here and there runs along every side of a field where the next tile is not the same field, on a ragged verge of rough grass left unploughed. */
vec4 Hedgerow(vec2 p, Grain grain)
{
	ivec2 tile = ivec2(floor(p));
	Ground home = GroundAt(tile);
	if (home.material != MAT_FIELDS) return vec4(0.0);
	vec2 f = fract(p);
	float near = 1.0;
	if (!SameField(tile - ivec2(1, 0), home)) near = min(near, f.x);
	if (!SameField(tile + ivec2(1, 0), home)) near = min(near, 1.0 - f.x);
	if (!SameField(tile - ivec2(0, 1), home)) near = min(near, f.y);
	if (!SameField(tile + ivec2(0, 1), home)) near = min(near, 1.0 - f.y);
	float width = HEDGE_WIDTH * (0.6 + 0.8 * grain.fine);
	float gaps = smoothstep(0.25, 0.4, Noise(p * HEDGE_GAPS_PER_TILE + 4.4));
	float pixel = 1.0 / tile_pixels;
	float hedge = Stroke(near, width, pixel) * gaps;
	float fringe = Stroke(near, width * HEDGE_FRINGE, pixel) * gaps * (1.0 - hedge);
	float verge = Stroke(near, VERGE_WIDTH * Varied(grain.local, 0.8), pixel);
	vec4 grass = vec4(VERGE * Varied(grain.micro, 0.5) * Varied(grain.clump, 0.3), 1.0) * verge * VERGE_OPACITY;
	vec4 leaves = vec4(HEDGE * Varied(grain.micro, 0.6) * Varied(grain.clump, 0.4), 1.0) * hedge * HEDGE_OPACITY;
	return Over(Over(grass, vec4(0.0, 0.0, 0.0, fringe * HEDGE_FRINGE_SHADE)), leaves);
}

/* Fine bumps and stones catch the light up close: soft on grass and fields, sharp on scree and rock. */
vec3 Roughened(vec3 normal, Detail detail, vec2 rugged)
{
	return normalize(normal - (detail.slope * rugged.x + detail.stone_slope * rugged.y) * RELIEF_DEPTH);
}

float CavityOpenness(Grain grain, vec2 rugged)
{
	float bump_hollow = max(BUMP_MEAN_HEIGHT - dot(vec2(grain.fine, grain.micro), BUMP_HEIGHT_SHARES), 0.0);
	return max(1.0 - dot(vec2(bump_hollow, max(grain.hollow, 0.0)) * rugged, CAVITY_OCCLUSION), 0.0);
}

/* Whether no tile within the reach the water's field weighs about a tile holds any water, from the water's texels eight tiles to a texel, each of which keeps any water under it however little. */
bool DryAbout(ivec2 tile)
{
	ivec2 low = Clamped(tile - DRY_REACH) >> DRY_LEVEL;
	ivec2 high = Clamped(tile + DRY_REACH) >> DRY_LEVEL;
	float water = texelFetch(u_water, low, DRY_LEVEL).r + texelFetch(u_water, ivec2(high.x, low.y), DRY_LEVEL).r;
	water += texelFetch(u_water, ivec2(low.x, high.y), DRY_LEVEL).r + texelFetch(u_water, high, DRY_LEVEL).r;
	return water <= 0.0;
}

Water WaterAt(vec2 p)
{
	vec4 near = DryAbout(ivec2(floor(p))) ? vec4(0.0) : WaterField(p);
	float wide = textureLod(u_water, p / MapSize(), DEPTH_LOD).r;
	float edge = max(fwidth(near.r), 1e-3);

	Water water;
	water.level = near.r;
	water.cover = smoothstep(WATERLINE - edge, WATERLINE + edge, near.r);
	water.depth = smoothstep(0.55, 1.0, wide) * smoothstep(0.5, 0.9, near.r);
	water.share = near.gba / max(near.r, 1e-3);
	water.sea = near.g;
	water.canal = near.b;
	return water;
}

/* A river runs over pebbled gravel in its shallows and over mud down its deep middle. */
vec3 Riverbed(Grain grain, float sunk)
{
	vec3 gravel = mix(RIVER_GRAVEL, PEBBLES, smoothstep(PEBBLE_EDGES.x, PEBBLE_EDGES.y, grain.stones));
	return mix(gravel, MUD, sunk);
}

/* Ground near water is damp, darkening toward the waterline; a river or a lake soaks its banks darker and further back than the sea's spray does. */
vec3 Banks(vec3 land, Water water, vec3 beach, Grain grain)
{
	float inland = 1.0 - clamp(water.share.x, 0.0, 1.0);
	float level = water.level + (grain.local - 0.5) * BANK_SWAY * inland;
	vec2 soaking = mix(SHORE_DAMP, BANK_DAMP, inland);
	vec3 damp = land * (1.0 - soaking.x * smoothstep(soaking.y, DAMP_LEVEL, level));
	float gravelled = Landscape() == LANDSCAPE_TOYLAND ? 0.0 : inland;
	float strewn = smoothstep(BANK_GRAVEL_LEVELS.x, BANK_GRAVEL_LEVELS.y, level + (grain.stones - 0.5) * BANK_GRAVEL_RAGGED) * gravelled;
	damp = mix(damp, RIVER_GRAVEL * WET_GRAVEL_SHADE, strewn);
	vec3 walled = mix(damp, BANK, smoothstep(0.3, 0.46, water.canal));
	return mix(walled, beach, smoothstep(SANDED_SEA, 0.44, water.sea));
}

/* The ground under water: wet sand along sea shores giving way to silt in the deep, a riverbed under rivers and dressed stone in canals. */
vec3 Seabed(Water water, Grain grain, float sunk)
{
	float sea = clamp(water.share.x, 0.0, 1.0);
	float canal = clamp(water.share.y, 0.0, 1.0 - sea);
	float river = 1.0 - sea - canal;
	vec3 bed = mix(WET_SAND, SILT, water.depth) * sea + CANAL_FLOOR * canal + Riverbed(grain, sunk) * river;
	return bed * (0.92 + 0.16 * grain.local) * Varied(grain.micro, 0.12);
}

ForestFloor FloorAt(ivec2 tile)
{
	uvec4 codes = CodesAt(tile);
	float cover = FloraCover(codes);
	if (cover <= 0.0 || (codes.g & GARDEN_BIT) != 0u) return ForestFloor(cover, vec4(0.0));
	uint kind = codes.b >> FLORA_KIND_SHIFT;
	return ForestFloor(cover, vec4(TREE_LITTER[kind], 1.0) * TREE_LITTER_COVERS[kind] * cover);
}

ForestFloor Mixed(ForestFloor a, ForestFloor b, float share)
{
	return ForestFloor(mix(a.cover, b.cover, share), mix(a.litter, b.litter, share));
}

ForestFloor FloorAround(vec2 p)
{
	vec2 q = p - TILE_CENTRE;
	ivec2 cell = ivec2(floor(q));
	vec2 f = smoothstep(0.0, 1.0, fract(q));
	return Mixed(Mixed(FloorAt(cell), FloorAt(cell + ivec2(1, 0)), f.x), Mixed(FloorAt(cell + ivec2(0, 1)), FloorAt(cell + ivec2(1, 1)), f.x), f.y);
}

vec3 CanopyTone(vec2 p)
{
	uvec4 home = CodesAt(ivec2(floor(p)));
	uint kind = TreeCount(home) > 0 ? home.b >> FLORA_KIND_SHIFT : TREE_BROADLEAF;
	return TREE_CANOPY[kind] * Varied(Octave(p, 1.1), CANOPY_VARIETY);
}

/* The ground under a forest: kept from the sky by the crowns, darkened between them by their shade as distance shrinks the trees, and painted their colour where it has thinned the trees themselves away. */
Canopy Forest(vec2 p)
{
	float density = FloorAround(p).cover;
	if (density <= 0.0) return Canopy(vec4(0.0), 0.0, 1.0);

	float pixels = TilePixelsAt(RenderPoint(v_world));
	float tint = ForestTint(pixels);
	return Canopy(vec4(CanopyTone(p), 1.0) * density * FOREST_COVER * tint, density, 1.0 - density * CROWN_SHADE * TreesShrinking(pixels));
}

Patch Littered(Patch ground, Canopy forest, vec2 p, Grain grain)
{
	if (forest.density <= 0.0) return ground;
	vec4 litter = FloorAround(p).litter;
	if (litter.a <= 0.0) return ground;

	float patches = mix(1.0 - LITTER_PATCHINESS, 1.0, smoothstep(LITTER_PATCH_EDGES.x, LITTER_PATCH_EDGES.y, grain.local));
	vec3 leaves = litter.rgb / litter.a * Varied(grain.fine, LITTER_VARIETY) * Varied(grain.micro, LITTER_VARIETY);
	vec3 tone = mix(leaves, LITTER_MOSS, smoothstep(MOSS_EDGES.x, MOSS_EDGES.y, Octave(p + MOSS_SEED, MOSS_PATCHES_PER_TILE)));
	tone = mix(CanopyTone(p) * FAR_FLOOR_SHADE, tone, smoothstep(LITTER_SHOWN_PIXELS.x, LITTER_SHOWN_PIXELS.y, tile_pixels));
	return Mixed(ground, Patch(tone, ROUGH_RUGGED), smoothstep(LITTER_DENSITY.x, LITTER_DENSITY.y, litter.a) * patches);
}

float GridLine(vec2 p, mat2 pixel)
{
	vec2 gap = abs(fract(p + HALF_TILE) - HALF_TILE);
	float x_edges = PixelLine(gap.x, Pitch(pixel, vec2(1.0, 0.0)), LINE_HALF_PIXELS);
	float y_edges = PixelLine(gap.y, Pitch(pixel, vec2(0.0, 1.0)), LINE_HALF_PIXELS);
	return max(x_edges, y_edges);
}

vec3 Overlay(vec3 colour, Network network)
{
	if (u_layer == LAYER_NONE) return colour;
	return mix(Greyed(colour), Accent(u_layer), u_layer == LAYER_RAIL ? network.layers.x : network.layers.y);
}

bool OutsideMap(vec2 p)
{
	ivec2 tile = ivec2(floor(p));
	return !OnMap(tile) || texelFetch(u_tiles, tile, 0).r == MAT_VOID;
}

/* Past the map's edge the bed deepens from what lies along the edge to the open sea's silt across the shelf, so it meets the bed inside without a seam. */
float OffshoreDepth(vec2 p)
{
	vec2 edge = clamp(p, vec2(0.0), MapSize());
	float wide = textureLod(u_water, edge / MapSize(), DEPTH_LOD).r;
	return mix(smoothstep(0.55, 1.0, wide), 1.0, smoothstep(0.0, SHELF_TILES, distance(p, edge)));
}

uint MarkAt(ivec2 tile)
{
	uvec4 codes = CodesAt(tile);
	return codes.r == MAT_PAVED ? codes.a : AIRFIELD_NONE;
}

bool Taxiable(uint mark)
{
	return mark == AIRFIELD_TAXIWAY || mark == AIRFIELD_RUNWAY || mark == AIRFIELD_STAND;
}

float AirfieldStroke(float gap, float half_width)
{
	return Stroke(gap, half_width, 1.0 / tile_pixels);
}

/* A runway lies along the axis its neighbouring runway tiles run; it carries edge lines, a dashed centre line, edge lights and, where it ends, threshold bars. */
vec4 Runway(ivec2 tile, vec2 f, vec2 p)
{
	bool along_x = MarkAt(tile + ivec2(1, 0)) == AIRFIELD_RUNWAY || MarkAt(tile - ivec2(1, 0)) == AIRFIELD_RUNWAY;
	ivec2 onward = along_x ? ivec2(1, 0) : ivec2(0, 1);
	float along = along_x ? f.x : f.y;
	float across = abs((along_x ? f.y : f.x) - HALF_TILE);
	float run = along_x ? p.x : p.y;

	float white = AirfieldStroke(abs(across - RUNWAY_EDGE), RUNWAY_EDGE_HALF);
	float dash = step(fract(run * CENTRELINE_DASHES), 0.55);
	white = max(white, AirfieldStroke(across, CENTRELINE_HALF) * dash);
	bool ends_ahead = MarkAt(tile + onward) != AIRFIELD_RUNWAY;
	bool ends_behind = MarkAt(tile - onward) != AIRFIELD_RUNWAY;
	float inward = ends_ahead ? along : (ends_behind ? 1.0 - along : 0.0);
	if ((ends_ahead || ends_behind) && across < THRESHOLD_REACH && inward > THRESHOLD_FROM && inward < THRESHOLD_TO) {
		white = max(white, AirfieldStroke(abs(fract(across * THRESHOLD_BARS) - HALF_TILE), 0.25));
	}
	vec3 colour = mix(RUNWAY_ASPHALT, AIRFIELD_WHITE, white);
	float light = 1.0 - smoothstep(EDGE_LIGHT_RADIUS * 0.5, EDGE_LIGHT_RADIUS, length(vec2(fract(run * EDGE_LIGHTS) - HALF_TILE, (across - EDGE_LIGHT_LATERAL) * EDGE_LIGHTS) / EDGE_LIGHTS));
	return vec4(mix(colour, EDGE_LIGHT, light * Resolved(EDGE_LIGHTS * 2.0)), 1.0);
}

/* A taxiway's yellow centre line runs from the tile's middle toward each neighbour aircraft taxi on to. */
float TaxiLines(ivec2 tile, vec2 f)
{
	vec2 off = f - HALF_TILE;
	float line = 0.0;
	if (Taxiable(MarkAt(tile + ivec2(1, 0))) && off.x > -TAXI_LINE_HALF) line = max(line, AirfieldStroke(abs(off.y), TAXI_LINE_HALF));
	if (Taxiable(MarkAt(tile - ivec2(1, 0))) && off.x < TAXI_LINE_HALF) line = max(line, AirfieldStroke(abs(off.y), TAXI_LINE_HALF));
	if (Taxiable(MarkAt(tile + ivec2(0, 1))) && off.y > -TAXI_LINE_HALF) line = max(line, AirfieldStroke(abs(off.x), TAXI_LINE_HALF));
	if (Taxiable(MarkAt(tile - ivec2(0, 1))) && off.y < TAXI_LINE_HALF) line = max(line, AirfieldStroke(abs(off.x), TAXI_LINE_HALF));
	return line;
}

vec3 Concrete(vec2 f)
{
	vec2 joints = abs(fract(f * PANEL_JOINTS + HALF_TILE) - HALF_TILE) / PANEL_JOINTS;
	float joint = max(AirfieldStroke(joints.x, PANEL_JOINT_HALF), AirfieldStroke(joints.y, PANEL_JOINT_HALF));
	return APRON_CONCRETE * (1.0 - 0.18 * joint * Resolved(PANEL_JOINTS * 2.0));
}

/* An airport's paved tiles are painted as what they are: runways, taxiways, stands and helipads on concrete aprons. */
vec4 Airfield(vec2 p)
{
	ivec2 tile = ivec2(floor(p));
	uint mark = MarkAt(tile);
	if (mark == AIRFIELD_NONE) return vec4(0.0);
	vec2 f = fract(p);
	if (mark == AIRFIELD_RUNWAY) return Runway(tile, f, p);

	vec3 colour = Concrete(f);
	vec2 off = abs(f - HALF_TILE);
	if (mark == AIRFIELD_TAXIWAY || mark == AIRFIELD_STAND) colour = mix(colour, TAXI_YELLOW, TaxiLines(tile, f));
	if (mark == AIRFIELD_STAND) colour = mix(colour, TAXI_YELLOW, AirfieldStroke(abs(max(off.x, off.y) - STAND_BOX), TAXI_LINE_HALF));
	if (mark == AIRFIELD_HELIPAD) {
		float ring = AirfieldStroke(abs(length(f - HALF_TILE) - HELIPAD_RING), HELIPAD_LINE_HALF);
		float letter = max(AirfieldStroke(abs(off.x - 0.1), HELIPAD_LINE_HALF) * step(off.y, 0.15), AirfieldStroke(off.y, HELIPAD_LINE_HALF) * step(off.x, 0.1));
		colour = mix(colour, AIRFIELD_WHITE, max(ring, letter));
	}
	return vec4(colour, 1.0);
}

float LevelToward(vec2 step)
{
	ivec2 tile = Clamped(ivec2(floor(v_world.xy + step)));
	return FacetLevel(SurfaceOf(tile), clamp(v_world.xy - vec2(tile), 0.0, 1.0));
}

vec3 Damp(vec3 stone, float along, float rise)
{
	float reach = DAMP_REACH * Varied(Noise(vec2(along * DAMP_RAGS_PER_TILE, DAMP_SEED)), DAMP_RAGGED);
	return stone * mix(DAMP_STONE, vec3(1.0), smoothstep(0.0, reach, rise));
}

vec3 Coped(vec3 stone, float along, float drop)
{
	float pixel = 1.0 / tile_pixels;
	float cap = 1.0 - smoothstep(COPING_DEPTH - pixel, COPING_DEPTH + pixel, drop);
	float drip = Stroke(abs(drop - COPING_DEPTH - DRIP_WIDTH), DRIP_WIDTH, pixel);
	float reach = mix(STREAK_REACHES.x, STREAK_REACHES.y, Noise(vec2(along * STREAK_LENGTHS_PER_TILE, STREAK_SEEDS.y)));
	float streaks = smoothstep(STREAK_EDGES.x, STREAK_EDGES.y, Noise(vec2(along * STREAKS_PER_TILE, STREAK_SEEDS.x))) * (1.0 - smoothstep(0.0, reach, drop)) * Resolved(STREAKS_PER_TILE);
	vec3 face = stone * mix(1.0, STREAK_SHADE, streaks) * (1.0 - DRIP_SHADE * drip);
	return mix(face, MASONRY * COPING_LIFT, cap);
}

/* Only foundations part the ground into walls, and they are laid in dressed stone courses. */
vec3 WallFace(vec3 normal)
{
	float along = dot(v_world.xy, vec2(-normal.y, normal.x));
	float course = v_world.z * LevelRise() * COURSES_PER_TILE;
	float block = along * BLOCKS_PER_TILE + 0.5 * floor(course);
	vec2 joint = abs(fract(vec2(block, course)) - 0.5);
	float mortar = smoothstep(0.5 - MORTAR_SHARE, 0.5, max(joint.x, joint.y));
	float detail = Resolved(COURSES_PER_TILE);
	vec3 stone = MASONRY * Varied(Hash(ivec2(floor(block), floor(course))), 0.18 * detail) * (1.0 - 0.3 * mortar * detail);
	stone = Damp(stone, along, (v_world.z - LevelToward(normal.xy * HALF_TILE)) * LevelRise());
	if (v_mark > RETAINING_MARK) stone = Coped(stone, along, (LevelToward(-normal.xy * HALF_TILE) - v_world.z) * LevelRise());
	return stone;
}

vec3 EarthStratum(int layer)
{
	if (Landscape() == LANDSCAPE_TOYLAND) return ToyStratum(layer);
	int pick = int(Hash(ivec2(layer, 23)) * float(EARTH_STRATA_COUNT)) % EARTH_STRATA_COUNT;
	return EARTH_STRATA[pick] * Varied(Hash(ivec2(layer, 29)), EDGE_LAYER_VARIETY);
}

/* The strata blended as they read from too far to make out one from another. */
vec3 EarthMean()
{
	if (Landscape() == LANDSCAPE_TOYLAND) return ToyStrataMean();
	return (EARTH_STRATA[0] + EARTH_STRATA[1] + EARTH_STRATA[2] + EARTH_STRATA[3] + EARTH_STRATA[4]) / float(EARTH_STRATA_COUNT);
}

/* The level of the ground along the top of the map's edge, on the tile just inside it. */
float EdgeTop(vec3 normal)
{
	return LevelToward(-normal.xy * HALF_TILE);
}

/* How far below the ground's edge a band reaching this far down still shows, its foot ragged with roots and wavering along the cut. */
float Fringe(float along, float below, float reach, float pixels)
{
	float ragged = reach * Varied(Noise(vec2(along * EDGE_TOPSOIL_SWAY, 8.1)), EDGE_TOPSOIL_SWING) * Varied(OctaveAt(vec2(along, 2.9), EDGE_ROOTS_PER_TILE, pixels), EDGE_ROOTS_RAGGED);
	return 1.0 - smoothstep(ragged * 0.4, ragged, below);
}

/* The map's edge is cut through the land as a face of earth: the turf of the ground above hanging over its lip, dark topsoil, then layers of soil and rock wavering a little along it, dark with wet where the sea washes it. */
vec3 EdgeFace(vec3 normal, float pixels, vec3 turf)
{
	float along = dot(v_world.xy, vec2(-normal.y, normal.x));
	float depth = v_world.z * LevelRise();
	float bedding = (depth + (Noise(vec2(along * EDGE_STRATA_SWAY, 4.3)) - 0.5) * EDGE_STRATA_WARP) * EDGE_STRATA_PER_TILE;
	int layer = int(floor(bedding));
	float bands = ResolvedAt(EDGE_STRATA_PER_TILE, pixels);
	float below = (EdgeTop(normal) - v_world.z) * LevelRise();
	float dry = smoothstep(0.0, EDGE_WET_REACH, depth);
	vec3 earth = mix(mix(EarthMean(), EarthStratum(layer), bands), Soil() * EDGE_TOPSOIL_SHADE, Fringe(along, below, EDGE_TOPSOIL, pixels) * dry);
	float seam = mix(1.0, mix(EDGE_SEAM_SHADE, 1.0, smoothstep(0.0, 0.25, fract(bedding))), bands);
	float grain = Varied(OctaveAt(vec2(along, depth), EDGE_GRAIN_PER_TILE, pixels), 0.25);
	float wet = mix(EDGE_WET_SHADE, 1.0, smoothstep(0.0, EDGE_WET_REACH, depth));
	return mix(earth * seam * grain * wet, turf, Fringe(along + 5.3, below, EDGE_TURF, pixels) * dry);
}

vec3 Windblown(vec2 p, float along, float across)
{
	vec2 frequency = vec2(along, across);
	vec3 noise = NoiseSlope(vec2(dot(p, WIND), dot(p, ACROSS_WIND)) * frequency);
	vec2 slope = noise.yz * frequency;
	return vec3(noise.x, WIND * slope.x + ACROSS_WIND * slope.y);
}

float Glint(vec2 p, vec3 normal)
{
	if (tile_pixels <= GLINT_FROM_PIXELS) return 0.0;
	float level = floor(log2(max(tile_pixels / GLINT_PIXELS, 1.0)));
	vec2 q = p * exp2(level);
	ivec2 cell = ivec2(floor(q)) + ivec2(int(level) * GLINT_LEVEL_STRIDE, 0);
	vec2 dice = Hash2(cell);
	if (dice.x > GLINT_SHARE) return 0.0;
	vec2 spot = Hash2(cell + ivec2(0, GLINT_LEVEL_STRIDE));
	float point = 1.0 - smoothstep(0.08, 0.3, length(fract(q) - 0.25 - 0.5 * spot));
	vec3 facet = normalize(normal + vec3((Hash2(cell + ivec2(GLINT_LEVEL_STRIDE, 0)) - 0.5) * GLINT_SPREAD, 0.0));
	vec3 half_way = normalize(SunDirection() + ToEye(RenderPoint(v_world)));
	return point * pow(max(dot(facet, half_way), 0.0), GLINT_FOCUS) * smoothstep(GLINT_FROM_PIXELS, GLINT_TO_PIXELS, tile_pixels);
}

/* Across miles a blanket lies in broad sheets, snow wind packed grey and blue in some and fresh in others, sand baked darker in some and pale in others. */
float Broad(vec2 p)
{
	return smoothstep(0.35, 0.72, Layered(p + 31.0, BROAD_FREQUENCY));
}

Blanket Snowfield(Site site, float cover)
{
	vec2 p = site.p;
	vec3 drift = Windblown(p + 3.7, DRIFT_ALONG, DRIFT_ACROSS);
	vec2 slope = drift.yz * DRIFT_HEIGHT * Resolved(DRIFT_ACROSS);
	float ridges = Resolved(RIDGE_ACROSS);
	if (ridges > 0.0) slope += Windblown(p + 1.3, RIDGE_ALONG, RIDGE_ACROSS).yz * RIDGE_HEIGHT * ridges;
	float sastrugi = Resolved(SASTRUGI_ACROSS);
	if (sastrugi > 0.0) slope += Windblown(p, SASTRUGI_ALONG, SASTRUGI_ACROSS).yz * SASTRUGI_HEIGHT * sastrugi;
	float scoured = smoothstep(0.55, 0.85, Octave(p + 9.1, 0.7)) * (1.0 - drift.x);
	vec3 tone = mix(SNOW, SCOURED_SNOW, scoured) * mix(0.95, 1.02, drift.x) * Varied(site.grain.fine, 0.05) * Varied(site.grain.micro, 0.06);
	tone = mix(tone, tone * PACKED_SNOW_TINT, Broad(p));
	return Blanket(cover, tone, slope, SNOW_RUGGED, SNOW_ROUGHNESS, 1.0);
}

Dune DuneTrain(vec2 p, vec2 wind, float spacing, float seed)
{
	vec3 warp_x = NoiseSlope(p * DUNE_WARP_FREQUENCY + seed);
	vec3 warp_y = NoiseSlope(p * DUNE_WARP_FREQUENCY + seed + 31.7);
	float reach = 2.0 * DUNE_WARP;
	vec2 q = p + (vec2(warp_x.x, warp_y.x) - 0.5) * reach;
	vec3 stretch = NoiseSlope(OctaveTurn(DUNE_STRETCH_FREQUENCY) * p * DUNE_STRETCH_FREQUENCY + seed + 19.3);
	float spread = 2.0 * DUNE_STRETCH * spacing;
	vec2 heading = wind + reach * DUNE_WARP_FREQUENCY * (wind.x * warp_x.yz + wind.y * warp_y.yz) + spread * DUNE_STRETCH_FREQUENCY * (stretch.yz * OctaveTurn(DUNE_STRETCH_FREQUENCY));
	float along = dot(q, wind) + (stretch.x - 0.5) * spread;
	float s = fract(along / spacing);
	bool windward = s < DUNE_CREST;
	float t = windward ? s / DUNE_CREST : (1.0 - s) / (1.0 - DUNE_CREST);
	float rate = (windward ? 1.0 / DUNE_CREST : -1.0 / (1.0 - DUNE_CREST)) / spacing;
	float profile = t * t * (3.0 - 2.0 * t);
	vec3 swell = NoiseSlope(p * DUNE_SWELL_FREQUENCY + seed + 5.3);
	float u = clamp((swell.x - DUNE_CALM) / (1.0 - 2.0 * DUNE_CALM), 0.0, 1.0);
	float amplitude = u * u * (3.0 - 2.0 * u);
	vec2 swelling = 6.0 * u * (1.0 - u) / (1.0 - 2.0 * DUNE_CALM) * DUNE_SWELL_FREQUENCY * swell.yz;
	vec2 slope = 6.0 * t * (1.0 - t) * rate * heading * amplitude + profile * swelling;
	return Dune(profile * amplitude, slope, along, heading, windward ? 0.0 : 1.0);
}

struct Ripples {
	float shade;
	vec2 slope;
};

Ripples RipplesAlong(Dune dune, float bent, float per_tile)
{
	float phase = (dune.along + bent) * per_tile * TAU;
	return Ripples(sin(phase), cos(phase) * dune.heading * per_tile * TAU * RIPPLE_HEIGHT);
}

/* The two trains of dunes, and the ripples running across each, take turns to lead from one stretch of desert to the next, so neither repeats unbroken across it. */
Blanket Sandfield(Site site, float cover)
{
	vec2 p = site.p;
	Dune dune = DuneTrain(p, WIND, DUNE_SPACING, 0.0);
	Dune cross = DuneTrain(p, CROSS_WIND, DUNE_SPACING * CROSS_SPACING, 13.1);
	float share = mix(CROSS_SHARE_LOW, CROSS_SHARE_HIGH, smoothstep(0.3, 0.7, Noise(OctaveTurn(CROSS_SHARE_FREQUENCY) * p * CROSS_SHARE_FREQUENCY + 5.9)));
	float height = (dune.height + cross.height * share) / (1.0 + share);
	vec2 dune_slope = (dune.slope + cross.slope * share) * DUNE_HEIGHT;

	vec2 wobble = vec2(Noise(p * 0.9 + 2.1), Noise(p * 1.3 + 7.7)) - 0.5;
	float crossing = smoothstep(0.35, 0.65, Noise(OctaveTurn(RIPPLE_BLEND_FREQUENCY) * p * RIPPLE_BLEND_FREQUENCY + 1.7));
	Ripples along = RipplesAlong(dune, dot(wobble * RIPPLE_WARP, dune.heading), RIPPLES_PER_TILE);
	Ripples athwart = RipplesAlong(cross, dot(wobble * RIPPLE_WARP, cross.heading), CROSS_RIPPLES_PER_TILE);
	Ripples ripples = Ripples(mix(along.shade, athwart.shade, crossing), mix(along.slope, athwart.slope, crossing));
	float lee = mix(dune.lee, cross.lee, crossing);

	float gravel = smoothstep(0.72, 0.86, (1.0 - height) * 0.3 + Octave(p + 4.1, 0.45) * 0.7 + (site.grain.fine - 0.5) * 0.15);
	float rippled = Resolved(RIPPLES_PER_TILE) * (1.0 - 0.7 * lee) * (1.0 - gravel);
	vec2 slope = dune_slope * (1.0 - 0.6 * gravel) + ripples.slope * rippled;
	vec3 sand = mix(SAND, RED_SAND, Layered(p + 8.3, 0.05)) * mix(0.93, 1.04, height) * (1.0 + RIPPLE_SHADE * ripples.shade * rippled) * Varied(site.grain.micro, 0.08);
	sand = mix(sand, sand * BAKED_SAND_TINT, Broad(p));
	vec3 stones = GRAVEL * Varied(site.grain.local, 0.15) * Varied(site.grain.micro, 0.3) * mix(0.9, 1.12, site.grain.stones);
	return Blanket(cover, mix(sand, stones, gravel), slope, mix(SAND_RUGGED, SCREE_RUGGED, gravel * 0.6), SAND_ROUGHNESS, 0.0);
}

Blanket BlanketOver(Site site)
{
	if (site.climate.blanket <= 0.0) return NO_BLANKET;
	float fray = (Octave(site.p + 2.7, BLANKET_FRAY_FREQUENCY) - 0.5) * BLANKET_FRAY + (site.grain.broad - 0.5) * BLANKET_MEANDER;
	float edge = Landscape() == LANDSCAPE_ARCTIC ? SNOW_EDGE : SAND_EDGE;
	float lying = smoothstep(0.5 - edge, 0.5 + edge, site.climate.blanket + SnowLift(v_world.z + site.ragged * SNOW_LINE_RAGGED) + site.ragged + fray - site.trodden);
	float slip = smoothstep(BLANKET_SLIP_SLOPE, STEEPEST_SLOPE, site.relief.slope + site.ragged * 0.8);
	float clinging = smoothstep(0.55, 0.8, site.grain.stones * 0.6 + site.grain.fine * 0.4) * CLINGING;
	float cover = lying * (1.0 - slip * (1.0 - clinging));
	if (cover <= 0.0) return NO_BLANKET;
	return Landscape() == LANDSCAPE_ARCTIC ? Snowfield(site, cover) : Sandfield(site, cover);
}

struct Underfoot {
	Grain grain;
	Seams seams;
	uint lie;
	Site site;
	Patch ground;
	Blanket blanket;
};

Underfoot UnderfootAt(vec2 p, Grain grain, Footing footing)
{
	Underfoot underfoot;
	underfoot.grain = grain;
	underfoot.seams = SeamsAt(p);
	underfoot.lie = footing.lie;
	underfoot.ground = Surface(p, underfoot.seams, grain, footing, underfoot.site);
	underfoot.blanket = BlanketOver(underfoot.site);
	return underfoot;
}

/* The ground's own colour where it meets the map's edge, as it would lie level there. */
vec3 EdgeTurf(Underfoot underfoot)
{
	return mix(Altitude(underfoot.ground.tone), underfoot.blanket.tone, underfoot.blanket.cover);
}

float Drowned(Water water)
{
	return max(clamp(-v_mark / SUBMERGED_MARK, 0.0, 1.0), water.sea * smoothstep(0.0, DROWNED_LEVELS, -v_world.z));
}

bool Sunken(Water water)
{
	return Drowned(water) >= 1.0 && water.cover >= 1.0;
}

Footing EdgeFooting(float levels_per_pixel)
{
	return Footing(ReliefAt(UPRIGHT, levels_per_pixel), 0.0, LIE_DRY);
}

Footing FootingAt(vec2 p, Water water, Relief relief)
{
	if (Sunken(water)) return Footing(relief, 0.0, LIE_SUNKEN);
	return Footing(relief, water.sea > 0.0 ? Armoured(p) : 0.0, water.sea > SANDED_SEA ? LIE_BEACH : LIE_DRY);
}

/* Where the network's meshes stand opaque they hide its bands, which are then not worked out at all. */
Network BandsAt(vec2 p, mat2 pixel)
{
	if (tile_pixels > NETWORK_OPAQUE_PPT * HIDDEN_BAND_MARGIN) return Network(vec4(0.0), vec2(0.0));
	return NetworkAt(p, pixel);
}

vec3 Ashore(vec2 p, Water water, Underfoot underfoot, vec3 surface)
{
	vec3 beach = underfoot.lie == LIE_BEACH ? Shore(underfoot.site).tone : vec3(0.0);
	vec3 ground = Altitude(Composite(Composite(surface, Hedgerow(p + underfoot.seams.warp, underfoot.grain)), Airfield(p)));
	return Banks(mix(ground, underfoot.blanket.tone, underfoot.blanket.cover), water, beach, underfoot.grain);
}

Shade GroundShade(vec2 p, mat2 pixel, Water water, Detail detail, float levels_per_pixel, vec3 normal, Underfoot underfoot)
{
	Grain grain = underfoot.grain;
	Blanket blanket = underfoot.blanket;
	Canopy forest = Forest(p);
	Network network = BandsAt(p, pixel);
	float grid = clamp((tile_pixels - INFRASTRUCTURE_PPT) / GRID_FADE_PPT, 0.0, 1.0);
	Patch surface = Littered(underfoot.ground, forest, p, grain);
	float dry = 1.0 - water.cover;
	float blanketed = blanket.cover * dry;
	normal = normalize(normal - vec3(blanket.slope * blanketed, 0.0));
	vec2 rugged = mix(surface.rugged, blanket.rugged, blanket.cover) * dry;
	normal = Roughened(normal, detail, rugged);
	float drowned = Drowned(water);
	vec3 bed = Seabed(water, grain, smoothstep(0.0, RIVERBED_DEEP_MARK, -v_mark));
	vec3 land = underfoot.lie == LIE_SUNKEN ? bed : mix(Ashore(p, water, underfoot, surface.tone), bed, drowned);
	normal = normalize(mix(normal, UPRIGHT, drowned * water.sea * smoothstep(DROWNED_SHEER.x, DROWNED_SHEER.y, underfoot.site.relief.slope)));
	land *= 1.0 - (1.0 - grid) * u_contour * CONTOUR_DEPTH * Contour(v_world.z, levels_per_pixel) * (1.0 - water.cover);

	vec3 colour = Composite(land, network.paint);
	colour *= 1.0 - grid * u_grid * GRID_DEPTH * GridLine(p, pixel) * (1.0 - water.cover);
	colour = Composite(colour * forest.shade, forest.paint);
	float roughness = mix(GroundRoughness(), blanket.roughness, blanket.cover);
	float glint = blanket.glint > 0.0 ? Glint(p, normal) * blanketed * (1.0 - forest.paint.a) : 0.0;
	return Shade(Overlay(clamp(colour, 0.0, 1.0), network), normal, roughness, (1.0 - forest.density * CANOPY_OCCLUSION) * CavityOpenness(grain, rugged), glint);
}

Shade Plain(vec3 colour, vec3 normal)
{
	return Shade(Greyed(colour), normal, TERRAIN_ROUGHNESS, 1.0, 0.0);
}

/* Everything read through screen derivatives is read before the fragment branches, where neighbouring pixels may part ways.
 * Detail fades by the pixel's mean span, so ground seen at a glancing angle keeps its grain for the antialiasing to settle, and the finest by its longest. */
void main()
{
	vec2 p = v_world.xy;
	mat2 pixel = mat2(dFdx(p), dFdy(p));
	tile_pixels = 1.0 / max(sqrt(length(pixel[0]) * length(pixel[1])), LEAST_DIVISOR);
	footprint = pixel;
	float levels_per_pixel = max(fwidth(v_world.z), LEAST_DIVISOR);
	Water water = WaterAt(p);
	vec3 normal = normalize(v_normal);
	Relief relief = ReliefAt(normal, levels_per_pixel);
	Detail detail = DetailAt(RenderPoint(v_world), normal);
	relief.steep *= 1.0 - clamp(v_mark / FILL_MARK, 0.0, 1.0);
	relief.steep *= 1.0 - smoothstep(CHANNEL_WALL_LEVELS.x, CHANNEL_WALL_LEVELS.y, water.level) * (1.0 - water.sea);
	float face_pixels = 1.0 / max(max(length(pixel[0]), length(pixel[1])), levels_per_pixel * LevelRise());

	bool edge = v_mark > EDGE_MARK && v_mark <= WALL_MARK;
	Grain grain = GrainAt(p, detail);

	Shade shade;
	if (!edge && OutsideMap(p)) {
		shade = Plain(Seabed(Water(1.0, 1.0, OffshoreDepth(p), vec3(1.0, 0.0, 0.0), 1.0, 0.0), grain, 1.0), normal);
	} else if (v_mark > WALL_MARK) {
		shade = Plain(WallFace(normal), normal);
	} else {
		Underfoot underfoot = UnderfootAt(p, grain, edge ? EdgeFooting(levels_per_pixel) : FootingAt(p, water, relief));
		if (edge) shade = Plain(EdgeFace(normal, face_pixels, EdgeTurf(underfoot)), normal);
		else shade = GroundShade(p, pixel, water, detail, levels_per_pixel, normal, underfoot);
	}
	frag_colour = Lit(shade);
}

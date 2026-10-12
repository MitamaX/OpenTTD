const float TEXELS = float(TEXELS_PER_TILE);
const vec3 DIM_ROOM = vec3(0.3, 0.29, 0.27);
const float DIM_ROOM_DAYLIGHT = 0.33;

const uint OPEN_NONE = 0u;
const uint OPEN_WINDOW = 1u;
const uint OPEN_SHOP = 2u;
const uint OPEN_DOOR = 3u;
const uint OPEN_GLASS_DOOR = 4u;
const uint OPEN_SHUTTER = 5u;

const uint MUNTIN_NONE = 0u;
const uint MUNTIN_CROSS = 1u;
const uint MUNTIN_SASH = 2u;
const uint MUNTIN_MULLION = 3u;
const uint MUNTIN_TRANSOM = 4u;
const uint MUNTIN_GRID = 5u;

/* An opening in texels: what fills it, how wide and tall it is, how far above its band it starts, its glazing bars, whether it is arched and whether it is rounded. */
struct Opening {
	uint kind;
	float width;
	float height;
	float sill;
	uint muntin;
	bool arch;
	bool rounded;
};

const Opening NO_OPENING = Opening(OPEN_NONE, 0.0, 0.0, 0.0, MUNTIN_NONE, false, false);
const Opening COTTAGE_WINDOW = Opening(OPEN_WINDOW, 7.0, 4.0, 2.0, MUNTIN_CROSS, false, false);
const Opening COTTAGE_DOOR = Opening(OPEN_DOOR, 5.0, 7.0, 0.0, MUNTIN_NONE, false, false);
const Opening TERRACE_WINDOW = Opening(OPEN_WINDOW, 4.0, 5.0, 1.0, MUNTIN_SASH, false, false);
const Opening TERRACE_DOOR = Opening(OPEN_DOOR, 4.0, 6.0, 0.0, MUNTIN_NONE, false, false);
const Opening FLAT_WINDOW = Opening(OPEN_WINDOW, 6.0, 4.0, 2.0, MUNTIN_TRANSOM, false, false);
const Opening FLAT_ENTRANCE = Opening(OPEN_GLASS_DOOR, 6.0, 7.0, 0.0, MUNTIN_NONE, false, false);
const Opening SHOP_UPPER_WINDOW = Opening(OPEN_WINDOW, 5.0, 4.0, 2.0, MUNTIN_MULLION, false, false);
const Opening SHOP_DISPLAY = Opening(OPEN_SHOP, 12.0, 7.0, 2.0, MUNTIN_NONE, false, false);
const Opening SHOP_DOOR = Opening(OPEN_GLASS_DOOR, 4.0, 9.0, 0.0, MUNTIN_NONE, false, false);
const Opening OFFICE_RIBBON = Opening(OPEN_WINDOW, 7.0, 3.0, 3.0, MUNTIN_NONE, false, false);
const Opening OFFICE_LOBBY = Opening(OPEN_WINDOW, 7.0, 8.0, 2.0, MUNTIN_TRANSOM, false, false);
const Opening OFFICE_ENTRANCE = Opening(OPEN_GLASS_DOOR, 7.0, 10.0, 0.0, MUNTIN_MULLION, false, false);
const Opening CURTAIN_PANEL = Opening(OPEN_WINDOW, 7.4, 6.0, 1.0, MUNTIN_NONE, false, false);
const Opening CURTAIN_LOBBY = Opening(OPEN_WINDOW, 7.4, 10.0, 1.0, MUNTIN_TRANSOM, false, false);
const Opening CURTAIN_ENTRANCE = Opening(OPEN_GLASS_DOOR, 7.4, 11.0, 0.0, MUNTIN_MULLION, false, false);
const Opening ARCHED_WINDOW = Opening(OPEN_WINDOW, 6.0, 10.0, 3.0, MUNTIN_SASH, true, false);
const Opening ARCHED_DOOR = Opening(OPEN_DOOR, 8.0, 13.0, 0.0, MUNTIN_MULLION, true, false);
const Opening FACTORY_WINDOW = Opening(OPEN_WINDOW, 11.0, 7.0, 5.0, MUNTIN_GRID, false, false);
const Opening CLERESTORY = Opening(OPEN_WINDOW, 22.0, 4.0, 6.0, MUNTIN_GRID, false, false);
const Opening LOADING_SHUTTER = Opening(OPEN_SHUTTER, 20.0, 13.0, 0.0, MUNTIN_NONE, false, false);
const Opening BAY_SHUTTER = Opening(OPEN_SHUTTER, 24.0, 14.0, 0.0, MUNTIN_NONE, false, false);

/* Indexed by window grid. */
const Opening UPPER_OPENINGS[WINDOW_GRIDS] = Opening[WINDOW_GRIDS](NO_OPENING, COTTAGE_WINDOW, TERRACE_WINDOW, FLAT_WINDOW, SHOP_UPPER_WINDOW, OFFICE_RIBBON, CURTAIN_PANEL, ARCHED_WINDOW, FACTORY_WINDOW, CLERESTORY);
const Opening GROUND_OPENINGS[WINDOW_GRIDS] = Opening[WINDOW_GRIDS](NO_OPENING, COTTAGE_WINDOW, TERRACE_WINDOW, FLAT_WINDOW, SHOP_DISPLAY, OFFICE_LOBBY, CURTAIN_LOBBY, ARCHED_WINDOW, FACTORY_WINDOW, BAY_SHUTTER);
const Opening ENTRANCES[WINDOW_GRIDS] = Opening[WINDOW_GRIDS](NO_OPENING, COTTAGE_DOOR, TERRACE_DOOR, FLAT_ENTRANCE, SHOP_DOOR, OFFICE_ENTRANCE, CURTAIN_ENTRANCE, ARCHED_DOOR, LOADING_SHUTTER, NO_OPENING);

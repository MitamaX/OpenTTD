const CITY_COUNT = 3;
const CITY_HOUSES = 800;
const TOWN_HOUSES = 120;
const RAIL_TOWN_COUNT = 5;
const BANK_GRANT = 500000000;
const FOREST_BLOCK = 8;
const RIVER_REACH = 3;
const IDLE_TICKS = 2220;

class ClimateGrower extends GSController {
	grown = false;

	function Start();
	function Save();
	function Load(version, data);
	function GrowTowns();
	function LargestTowns();
	function LayRails();
	function LayRoute(from, to);
	function RoutePath(from, to);
	function ReportViews();
	function Report(label, tile);
	function DensestForest();
	function BusiestRiver();
	function CoastNear(anchor);
	function HighestGround();
}

function ClimateGrower::Start()
{
	this.Sleep(1);
	if (!this.grown) {
		this.GrowTowns();
		this.LayRails();
		this.ReportViews();
		this.grown = true;
	}
	while (true) this.Sleep(IDLE_TICKS);
}

function ClimateGrower::Save()
{
	return { grown = this.grown };
}

function ClimateGrower::Load(version, data)
{
	if ("grown" in data) this.grown = data.grown;
}

function ClimateGrower::LargestTowns()
{
	local towns = GSTownList();
	towns.Valuate(GSTown.GetPopulation);
	towns.Sort(GSList.SORT_BY_VALUE, false);
	return towns;
}

function ClimateGrower::GrowTowns()
{
	local rank = 0;
	foreach (town, population in this.LargestTowns()) {
		GSTown.ExpandTown(town, rank < CITY_COUNT ? CITY_HOUSES : TOWN_HOUSES);
		rank++;
	}
}

function ClimateGrower::RoutePath(from, to)
{
	local x = GSMap.GetTileX(from);
	local y = GSMap.GetTileY(from);
	local path = [from];
	while (x != GSMap.GetTileX(to)) {
		x += GSMap.GetTileX(to) > x ? 1 : -1;
		path.append(GSMap.GetTileIndex(x, y));
	}
	while (y != GSMap.GetTileY(to)) {
		y += GSMap.GetTileY(to) > y ? 1 : -1;
		path.append(GSMap.GetTileIndex(x, y));
	}
	return path;
}

function ClimateGrower::LayRoute(from, to)
{
	local path = this.RoutePath(from, to);
	for (local i = 1; i < path.len() - 1; i++) GSRail.BuildRail(path[i - 1], path[i], path[i + 1]);
}

function ClimateGrower::LayRails()
{
	local company = GSCompany.ResolveCompanyID(GSCompany.COMPANY_FIRST);
	GSCompany.ChangeBankBalance(company, BANK_GRANT, GSCompany.EXPENSES_OTHER, GSMap.TILE_INVALID);
	local mode = GSCompanyMode(company);
	GSRail.SetCurrentRailType(GSRailTypeList().Begin());

	local previous = null;
	local counted = 0;
	foreach (town, population in this.LargestTowns()) {
		if (counted++ == RAIL_TOWN_COUNT) break;
		if (previous != null) this.LayRoute(GSTown.GetLocation(previous), GSTown.GetLocation(town));
		previous = town;
	}
}

function ClimateGrower::Report(label, tile)
{
	GSLog.Warning("VIEW " + label + " " + GSMap.GetTileX(tile) + " " + GSMap.GetTileY(tile));
}

function ClimateGrower::DensestForest()
{
	local best = GSMap.TILE_INVALID;
	local most = -1;
	for (local y = 0; y + FOREST_BLOCK <= GSMap.GetMapSizeY(); y += FOREST_BLOCK) {
		for (local x = 0; x + FOREST_BLOCK <= GSMap.GetMapSizeX(); x += FOREST_BLOCK) {
			local count = 0;
			for (local dy = 0; dy < FOREST_BLOCK; dy++) {
				for (local dx = 0; dx < FOREST_BLOCK; dx++) {
					if (GSTile.HasTreeOnTile(GSMap.GetTileIndex(x + dx, y + dy))) count++;
				}
			}
			if (count > most) {
				most = count;
				best = GSMap.GetTileIndex(x + FOREST_BLOCK / 2, y + FOREST_BLOCK / 2);
			}
		}
	}
	return best;
}

function ClimateGrower::BusiestRiver()
{
	local best = GSMap.TILE_INVALID;
	local most = -1;
	for (local y = RIVER_REACH; y < GSMap.GetMapSizeY() - RIVER_REACH; y++) {
		for (local x = RIVER_REACH; x < GSMap.GetMapSizeX() - RIVER_REACH; x++) {
			local tile = GSMap.GetTileIndex(x, y);
			if (!GSTile.IsRiverTile(tile)) continue;
			local count = 0;
			for (local dy = -RIVER_REACH; dy <= RIVER_REACH; dy++) {
				for (local dx = -RIVER_REACH; dx <= RIVER_REACH; dx++) {
					if (GSTile.IsRiverTile(GSMap.GetTileIndex(x + dx, y + dy))) count++;
				}
			}
			if (count > most) {
				most = count;
				best = tile;
			}
		}
	}
	return best;
}

function ClimateGrower::CoastNear(anchor)
{
	local best = GSMap.TILE_INVALID;
	local nearest = 1 << 30;
	for (local y = 0; y < GSMap.GetMapSizeY(); y++) {
		for (local x = 0; x < GSMap.GetMapSizeX(); x++) {
			local tile = GSMap.GetTileIndex(x, y);
			if (!GSTile.IsCoastTile(tile)) continue;
			local distance = GSMap.DistanceManhattan(tile, anchor);
			if (distance < nearest) {
				nearest = distance;
				best = tile;
			}
		}
	}
	return best;
}

function ClimateGrower::HighestGround()
{
	local best = GSMap.TILE_INVALID;
	local highest = -1;
	for (local y = 0; y < GSMap.GetMapSizeY(); y++) {
		for (local x = 0; x < GSMap.GetMapSizeX(); x++) {
			local tile = GSMap.GetTileIndex(x, y);
			local height = GSTile.GetMaxHeight(tile);
			if (height > highest) {
				highest = height;
				best = tile;
			}
		}
	}
	return best;
}

function ClimateGrower::ReportViews()
{
	local rank = 0;
	local largest = GSMap.TILE_INVALID;
	foreach (town, population in this.LargestTowns()) {
		if (rank == 0) largest = GSTown.GetLocation(town);
		if (rank < 3) this.Report("town" + rank + "_pop" + population, GSTown.GetLocation(town));
		rank++;
	}
	this.Report("river", this.BusiestRiver());
	this.Report("coast", this.CoastNear(largest));
	this.Report("forest", this.DensestForest());
	this.Report("peak", this.HighestGround());
}

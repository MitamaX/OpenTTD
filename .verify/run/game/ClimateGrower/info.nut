class ClimateGrower extends GSInfo {
	function GetAuthor() { return "verify"; }
	function GetName() { return "ClimateGrower"; }
	function GetShortName() { return "CLGR"; }
	function GetDescription() { return "Grows towns, lays rails and reports views on a fresh map."; }
	function GetVersion() { return 1; }
	function GetDate() { return "2026-10-09"; }
	function CreateInstance() { return "ClimateGrower"; }
	function GetAPIVersion() { return "15"; }
}

RegisterGS(ClimateGrower());

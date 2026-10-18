from pathlib import Path
SITES = {
 "a": [("susSea176",176.5,243.5),("susSea51",51.5,208.5),("susSea123",123.5,22.5),("susLand48",48.5,145.5),
       ("susTun112",112.5,152.5),("susLand41",41.5,133.5),("susLand111",111.5,104.5),("susRiver72",72.5,111.5),
       ("cantRail103",103.5,189.5),("cantSea195",194.5,244.5),("cantSea238",238.5,117.5),("cantRiver150",150.5,224.5),
       ("coastRail89",89.5,230.5),("coastRoad238",238.5,11.5)],
 "b": [("cantCross121",121.5,222.5),("cantRail216",216.5,102.5),("riverBridges18",18.5,120.5),("cantRoad42",42.5,108.5),
       ("roadRiver207",207.5,141.5),("railRail46",46.5,149.0),("railRoad58",58.5,149.5),("railRoad86",86.5,150.5),
       ("railRiver178",178.5,215.5),("concRail152",152.5,218.5),("cross220",220.5,93.5),("woodRail214",214.5,125.5),
       ("roadRail68",68.5,60.5),("girdRoadRail36",36.5,95.5)],
 "c": [("rapidsRoad26",26.5,107.5),("rapids6",6.5,54.5),("rapids65",65.5,212.5),("tun71",71.5,144.5),
       ("tun49",49.5,135.5),("tun87",87.5,183.5),("tun97",97.5,143.5),("tun57",57.5,122.5),
       ("tunRoad123",123.5,210.5),("tunRoad63",63.5,24.5),("tunRoad83",83.5,36.5),("slopeStation28",28.5,168.5),
       ("slopeRoad234",234.5,20.5),("roadSea219",219.5,223.5)],
}
NEAR = (240, 32)
LOW = (420, 16)
verify = Path(__file__).parent.parent
for group, sites in SITES.items():
    lines = ["wait 90"]
    for index, (name, x, y) in enumerate(sites):
        for step, yaw in enumerate(range(0, 360, 45)):
            zoom, pitch = LOW if (step + index) % 2 else NEAR
            lines.append(f"sw_{name}_y{yaw:03d} {x} {y} {zoom} {yaw} {pitch}")
    (verify / f"shots-sweep-{group}.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(group, len(lines) - 1)

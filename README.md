# PortfolioDemo
The terrain building is the most "complete" section of the Mineral Madness project that is mostly done by me. 

The marching cube handler contains a 3D grid of marching cubes. This allows smaller sections to be updated rather than requiring to rebuild the whole map each time.

I think a good decent part to look at is MarchingCubeHandler::runQueuedMarchingCubes(Physics& physics). This starts the process of updating the terrain. It calls runMarchingCubes(,,) on any section that needs to be updated.

A demo of the game can be tested at:
https://mega.nz/file/qglTzIQQ#EETvdnHb3zCR_L2bqUHDWXgFdnETK58nW-eCrCWIdWY
Just start MineralMadness.exe and then enter "sandbox" in game.

Sections not mine:
MarchingCubeData is mostly a copy-paste of tables from https://paulbourke.net/geometry/polygonise/.
The largest sections here that other team mates wrote are any octree, scanning, pathfinding, and raycast sections.

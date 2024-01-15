#pragma once
#include "L_System.h"

// forward declarations
class MarchingCubeHandler;

// Class responsibility will be to parse the L-System string into structure points, and then use those points to carve the terrain data.
// carving is temporarily done in cpu with MarchingCube::damageTerrain, which is just a spherical shape. 
// Intention is to move to warped 
class CaveCarver
{
public:
	struct StructurePoint
	{
		float3 pos;
		float radius;
		//float noise;
		StructurePoint(float3 in_pos, float in_radius)
		{
			pos = in_pos;
			radius = in_radius;
		}
	};
private:

	std::vector<StructurePoint> m_structurePoints;

public:
	void createStructurePoints(std::string str, float3 startPos, float3 dir);
	void carveData(MarchingCubeHandler& mc);
	void ajustPointsToTerrain(MarchingCubeHandler& mc);
	const std::vector<StructurePoint>& getStructurePoints() const;
};


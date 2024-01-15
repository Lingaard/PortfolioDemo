#pragma once
#include "MarchingCube.h"
#include "CullingTrees.h"
#include "DrawableOctree.h"
#include "CaveCarver.h"
#include "GameObject.h"
#include "Graphics.h"

// forward decarations
class Physics;
class PathfindingManager;

// Creates and handles the marching cubes and the terrain data
class MarchingCubeHandler : public GameObject
{
private:
	friend PathfindingManager;
public:
	struct CubeRayCastInfo {
		size_t totalCubes;
		size_t culledCubes;
		size_t rayIntersectedCubes;
		size_t loopIterations;
		size_t totalTriangles;
		size_t culledTriangles;
	};
private:
	static const int s_nrCubes = 16;
	static const int s_totalCubes = s_nrCubes * s_nrCubes * s_nrCubes;
	MarchingCube m_mcs[s_nrCubes][s_nrCubes][s_nrCubes];

	struct DecorCollection {
		std::string m_name;
		struct DecorInstance {
			float4x4 matrix;
			float3 color;
		};
		std::vector<DecorInstance> m_instances;
		DecorCollection(std::string name) : m_name(name) {}
	};
	std::vector<DecorCollection> m_decor;


	std::bitset<s_totalCubes> m_marchingCubeQueueLookup; // quick way of checking if cube data is updated. Used together with queue.
	std::vector<int3> m_marchingCubeQueue;
	std::list<int3> m_oldIDs;	// last frame's active IDs

	std::shared_ptr<DrawableOctree<MarchingCube*>> m_octree = std::make_shared<DrawableOctree<MarchingCube*>>(); // contains references to marching cube chunks
	CubeRayCastInfo m_rayInfo = { 0 };

	std::shared_ptr<TERRAINDATATYPE[]> m_terrainData;	// Basicly a 3D texture
	int m_sizeX;
	int m_sizeY;
	int m_sizeZ;
	int m_totalSize;

	float m_surfaceValue; // is right now hardcoded both here and in MarchingCube
	float m_destroyValue; // is right now hardcoded both here and in MarchingCube

	// Colors
	ConstantBuffer<TerrainColor> m_cbuffer_terrainColor;
	TerrainColor m_terrainColorData;

	// Scanner
	enum ScannerState {
		Scan_Inactive,
		Scan_Active,
		Scan_Fade
	} m_scanningState = Scan_Inactive;

	ConstantBuffer<ScanningProperties>& m_cbuffer_scannerProperties;
	const float m_scannerBaseMaxDistance = 7.0f;
	const float m_scannerPowerupMaxDistance = 10.f;
	float m_scannerMaxCooldown = 4.0f;
	float m_scannerCooldown = 0;
	bool m_waitForCooldown = false;
	float m_scannerAlphaTimer = 0; // keeps track of alpha fade animation
	float m_scannerActivationTimer = 0;
	const float m_scannerAlphaFadeTime = 0.5f; // time length of fade
	const float m_scannerMinimumActivationTime = 1.f; // time until alpha starts to fade
	float m_scannerAnimationTimer = 0; // keeps track of animation
	const float m_scannerAnimationLength = 1.5f; // how long the animation takes (in seconds)
	const float m_scannerForwardFalloffDistance = 0.25f; // additional distance to fade hologram edge
	float m_scannerMaxDistance = m_scannerBaseMaxDistance; // scanner range
	const float m_scannerAnimationPower = 4; // scanner speed
	bool m_scannerUnlimitedCooldown = false;

	std::vector<CaveCarver::StructurePoint> m_structurePoints;		// Structure points used in carving algorithm
	std::vector<float3> m_playerSpawnPositions;						// The generated player spawn positions

	unsigned int m_latestSeed = 0;
	struct ImGuiEditInfo {
		int seed = 0;
		int size = 60;
		float scale = 10.f;
	} editInfo;

private:

	void initDataTexture(int sizeX, int sizeY, int sizeZ);
	void setTerrainPixel(int x, int y, int z, TERRAINDATATYPE value);
	TERRAINDATATYPE getTerrainPixel(int x, int y, int z) const;
	TERRAINDATATYPE getTerrainPixel(float3 pos) const;

	float3 translateWorldToDataSpace(float3 worldPos) const;
	float3 translateWorldToLocalSpace(float3 worldPos) const;

	// Adds cube index to update queue.
	// Returns true if added to queue.
	bool queueMarchingCube(int3 cubeIdx);
	// Adds pixel's cube index to update queue and aand adjacent cubes the pixel is edgeing.
	// Returns true if added to queue.
	bool queueMarchingCube_pixelIndex(int3 pixelIdx);

	// override Drawable
	void _draw(const float4x4& matrix) override;
	void _update(double dt) override;

	void read_sceneNode_internal(std::ifstream& file) override;
	void write_sceneNode_internal(std::ofstream& file) override;

	float3 getTerrainColorFromLocalPosition(float3 localPosition, float3 normal);

	void eraseDecor_sphere(float3 worldPos, float radius);

public:
	MarchingCubeHandler();
	~MarchingCubeHandler();

	void imgui_edit() override;

	void init(int sizeX, int sizeY, int sizeZ, float scale);
	void initTerrainColorData();

	float3 getDataFieldFlow(float3 worldPos, float localGridStepSize = 1.f); // gets normal based on neighboring data cells, based on central difference
	float3 getDataFieldFlow(int3 pixelIdx, float localGridStepSize = 1.f); // gets normal based on neighboring data cells, based on central difference
	void visualizeDataField();

	// Octree
	void initOctree();
	/*
	Raycast against terrain mesh (good for long distance, use short raycast for short distances).
	Returns true if ray collided with any triangles.
	Parameter 'distance' defines the ray length and will also be overwritten by the rays collision distance.
	Parameters 'intersectionPosition' and 'intersectionNormal' will be overwritten by the rays intersection point and normal of collision surface.
	*/
	bool raycast(float3 rayPosition, float3 rayDirection, float& distance, float3& intersectionPosition, float3& intersectionNormal);
	bool longRaycast_localSpace(float3 rayPosition, float3 rayDirection, float& distance, float3& intersectionPosition, float3& intersectionNormal);
	/*
	Raycast against terrain mesh (Optimized for short distances).
	Returns true if ray collided with any triangles.
	Parameter 'distance' defines the ray length and will also be overwritten by the rays collision distance.
	Parameters 'intersectionPosition' and 'intersectionNormal' will be overwritten by the rays intersection point and normal of collision surface.
	*/
	bool shortRaycast(float3 rayPosition, float3 rayDirection, float& distance, float3& intersectionPosition, float3& intersectionNormal);
	bool shortRaycast_localSpace(float3 rayPosition, float3 rayDirection, float& distance, float3& intersectionPosition, float3& intersectionNormal);

	bool raycast_localSpace(float3 rayPosition, float3 rayDirection, float& distance, float3& intersectionPosition, float3& intersectionNormal) override;

	/* Returns information on latest raycast call */
	CubeRayCastInfo getRayCastInfo() const;
	/* Returns the distance between two points that is obstructed by the terrain */
	float measureWallThickness(float3 point1, float3 point2);

	// MC handling
	void initCubes();

	// Mech creating
	void runAllMarchingCubes(Physics& physics);
	void runQueuedMarchingCubes(Physics& physics);
	// without physics
	void runAllMarchingCubes();
	void runQueuedMarchingCubes();

	// data reading
	void setTerrainData(int sizeX, int sizeY, int sizeZ, TERRAINDATATYPE arr[]);
	void setTerrainData(int sizeX, int sizeY, int sizeZ, std::shared_ptr<TERRAINDATATYPE[]> sp);

	const std::vector<CaveCarver::StructurePoint>& getStructurePoints() const;
	const std::vector<float3>& getPlayerSpawnPositions() const;

	void drawStructurePoints();

	// Generates data. 
	void generateData_sphere();
	void generateData_cheese();
	void generateData_fill();

	void generateData_testCave(int nrOfPlayers = 2, unsigned int seed = rand());

	void placeDecor();

	// Destroy terrain in a sphere. Radius unit is in data cells
	void destroySphere(float3 worldPos, float worldRadius);
	void damageSphere(float3 worldPos, float worldRadius, float smoothingDataRange = 1.f);
	void damageCylinder(float3 worldPos, float radius = 0.5f, float height = 0.5f, TERRAINDATATYPE strength = 180);
	void smoothTerrain();

	// check terrain (maybe used for terrain interactions)
	float getTerrainValue(float3 worldPos) const;
	bool isInGround(float3 worldPos);
	bool isOnEdge(int3 nodeIndex) const;
	bool isOnEdge(float3 worldPos) const;

	float getTriangleMeshSize();
	float getTerrainDataSize();

	// Sets the physics of cubes near bombs active. 
	void updateCubesPhysicsActive();

	// Override Drawable
	virtual DirectX::BoundingBox getLocalBoundingBox() const { return DirectX::BoundingBox(); };

	/*
	Get scanners cooldown between [0, 1]. 0 = ready to use.
	Returns -1 if scanner is active.
	*/
	float getScannerCooldown() const;
	bool isScannerActive() const;
	bool isWaitingForScannerCooldown() const;
	void setScannerPowerupState(bool state);

	float3 getTerrainColor() const;
	void relayColorToGraphics();
};


#pragma once
#include "Drawable.h"
#include "PipelineState.h"
#include "CullingTrees.h"
#include "SimpleTypes.h"

class Physics;

#define TERRAINDATATYPE unsigned char
#define STR_VALUE(X) #X						// A lot of hoops to print out the terraintype macro. Used when I tried a scripted benchmarking session
#define TERRAINTYPE_NAME(X) STR_VALUE(X)
#define TERRAINDATATYPESTR TERRAINTYPE_NAME(TERRAINDATATYPE)

struct TerrainColor // GPU const buffer
{
	float4 colorFloor[4];
	float4 colorWall[4];
	float4 colorCeil[4];
	float2 colorLimits;
	float  terrainHightInverted = 0;
	float  padding = 0;
};

class MarchingCube : public DrawableObject
{
	struct VertexData {
		float3 position;
		float3 normal;
	};
	/* Set of vertices that forms a triangle */
	struct Triangle {
		VertexData points[3];
		/* returns the plane normal of the triangle */
		float3 calcFlatNormal() const {
			float3 normal = (points[1].position - points[0].position).Cross(points[2].position - points[0].position);
			normal.Normalize();
			return normal;
		}
	};

private:
	// Terrain data
	static std::shared_ptr<TERRAINDATATYPE[]> s_terrainData;	// Basicly a 3D texture. This is a reference to the one in the handler
	int m_sizeX;	// How many data cells this marching cube uses
	int m_sizeY;	
	int m_sizeZ;	
		
	// Marching stuff
	float m_surfaceValue;		// At what density value a surface will be rendered
	float m_destroyValue;		// Set value to this after when destroyed

	// Graphics
	std::shared_ptr< PipelineInstance> m_pipelineInstance_terrain = std::make_shared<PipelineInstance>(PipelineStateIdentifier::State_MarchingCubes);
	std::shared_ptr< PipelineInstance> m_pipelineInstance_shadow = std::make_shared<PipelineInstance>(PipelineStateIdentifier::State_MarchingCubes);
	std::shared_ptr< PipelineInstance> m_pipelineInstance_scanning = std::make_shared<PipelineInstance>(PipelineStateIdentifier::State_MarchingCubeScanning);
	bool m_drawScanner = false;

	VertexBuffer<VertexData> m_vertexBuffer;

	Octree<Triangle> m_octreeMesh;

	// Handling stuff
	int3 m_startDataPos;
	static int s_nrCubes;

	bool m_simulationActive;
	physx::PxRigidDynamic* m_actor;


private:
	TERRAINDATATYPE getTerrainPixel(int x, int y, int z) const;
	//float sampleTerrain(float x, float y, float z) const;// interpolates values. More explensive but should get smoother diagonals

	float3 translateWorldToDataSpace(float3 worldPos);	// doesn't work right now as it doesn't account for the handler's transform.

	void singleMarchCube(int x, int y, int z);

	void fillOctree(const std::vector<VertexData>& vertices);

	void fillPipelineInstances();
	// override parents
	void _draw(const float4x4& matrix) override;
	std::vector<std::shared_ptr<PipelineInstanceBase>> _getShadowInstances(const float4x4 matrix) override;
public:
	MarchingCube();
	~MarchingCube();

	// mesh generation
	void runMarchingCubes(Physics& physics, const float4x4& matrix, const float3& scale);
	void runMarchingCubes();
	// handle stuff
	void setStartDataPos(int3 pos);
	static void setTerrainData(std::shared_ptr<TERRAINDATATYPE[]> data);
	static void setNrCubes(int nr);
	void setDataSizes(int x, int y, int z);
	void setDataSizes(int3 sizes);
	void setScannerState(bool state);
	void bindColorBuffer(ConstantBuffer<TerrainColor>& cbuffer);


	std::vector<float3> getVertexPositions();
	void clearVertexData();
	int getTriangleDataSize();
	void setPhysicsActive(bool active); 
	// override parents
	DirectX::BoundingBox getLocalBoundingBox() const override; // empty

	/*
	Returns true if ray collided with any triangles.
	Parameter 'distance' defines the ray length and will also be overwritten by the rays collision distance.
	Parameters 'intersectionPosition' and 'intersectionNormal' will be overwritten by the rays intersection point and normal of collision surface.
	*/
	bool raycast(float3 rayPosition, float3 rayDirection, float& distance, float3& intersectionPosition, float3& intersectionNormal, size_t& tests);
};

// TODO: Rewrite singleMarchingCube to use float coordinates + scale as input. 
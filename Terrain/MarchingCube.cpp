#include "pch.h"
#include "MarchingCube.h"
#include "MarchingCubeData.h"
#include "Graphics.h"
#include "Physics.h"
// init statics 
int MarchingCube::s_nrCubes = 10;
std::shared_ptr<TERRAINDATATYPE[]> MarchingCube::s_terrainData = nullptr;

TERRAINDATATYPE MarchingCube::getTerrainPixel(int x, int y, int z) const
{
	int3 totalSizes(m_sizeX * s_nrCubes, m_sizeY * s_nrCubes, m_sizeZ * s_nrCubes);
	int totalTotal = totalSizes.x * totalSizes.y * totalSizes.z;

	int index = (x + m_startDataPos.x) + (y + m_startDataPos.y) * totalSizes.x +
		(z + m_startDataPos.z) * totalSizes.x * totalSizes.y;
	//int index = x + y * m_sizeX + z * m_sizeX * m_sizeY;

	if (0 <= index && index < totalTotal)
		return s_terrainData[index];
	else
		return (TERRAINDATATYPE)255;
}

// Not sure if works since hanlder update. maybe
float3 MarchingCube::translateWorldToDataSpace(float3 pos)
{
	// translate worldPos to terrainData's local position. (Assumes terrain is not rotated)	
	float4x4 invWorldMat = getMatrix().Invert();

	// worldSpace [-inf, inf] to localSpace [0, 1]
	float3::Transform(pos, invWorldMat);
	float3 scale = getScale();

	// localSpace to dataSpace [0, m_sizeX/Y/Z]
	float3 dataSizes((float)m_sizeX, (float)m_sizeY, (float)m_sizeZ);
	pos *= dataSizes;
	return pos;
}

void MarchingCube::singleMarchCube(int x, int y, int z)
{
	float4 cubeCorners[8];	// corner with position in xyz and density value in w.
	float tx, ty, tz;			// temp position coordinates
	float3 cubeLengths = float3(1 / (float)m_sizeX, 1 / (float)m_sizeY, 1 / (float)m_sizeZ);

	for (size_t i = 0; i < 8; i++)
	{
		tx = x + MarchingCubeData::vertexOffset[i][0];
		ty = y + MarchingCubeData::vertexOffset[i][1];
		tz = z + MarchingCubeData::vertexOffset[i][2];
		TERRAINDATATYPE value = getTerrainPixel((int)tx, (int)ty, (int)tz);

		// Positions are scaled to make the whole marching cube unit length.
		cubeCorners[i] = DirectX::SimpleMath::Vector4(tx * cubeLengths.x, ty * cubeLengths.y, tz * cubeLengths.z, (float)value);
	}

	// create index flag that identifies which corners are "inside" the surface
	int cubeIndex = 0;
	for (int i = 0; i < 8; i++)
		if (cubeCorners[i].w < m_surfaceValue) { cubeIndex |= 1 << i; }

	// Create the triangels. 
	// triangleConnectionTable holds which edge points should be connected 
	// to create triangles, index is -1 if there is no more triangles in cube. 
	for (size_t i = 0; MarchingCubeData::triangleConnectionTable[cubeIndex][i] != -1; i += 3)
	{
		// Find 
		int edgeIndex = MarchingCubeData::triangleConnectionTable[cubeIndex][i];
		int a0 = MarchingCubeData::edgeConnection[edgeIndex][0];
		int b0 = MarchingCubeData::edgeConnection[edgeIndex][1];

		edgeIndex = MarchingCubeData::triangleConnectionTable[cubeIndex][i + 1];
		int a1 = MarchingCubeData::edgeConnection[edgeIndex][0];
		int b1 = MarchingCubeData::edgeConnection[edgeIndex][1];

		edgeIndex = MarchingCubeData::triangleConnectionTable[cubeIndex][i + 2];
		int a2 = MarchingCubeData::edgeConnection[edgeIndex][0];
		int b2 = MarchingCubeData::edgeConnection[edgeIndex][1];

		float3 tri[3];
		tri[0] = MarchingCubeData::pointLerp(cubeCorners[a1], cubeCorners[b1], m_surfaceValue);
		tri[1] = MarchingCubeData::pointLerp(cubeCorners[a0], cubeCorners[b0], m_surfaceValue);
		tri[2] = MarchingCubeData::pointLerp(cubeCorners[a2], cubeCorners[b2], m_surfaceValue);

		float3 norm = MarchingCubeData::getNormal(tri[0], tri[1], tri[2]);

		// if all points are in the same position (happens when middle point equals the surface value)
		if (norm == float3(0, 0, 0))
			continue;

		VertexData vertexData;

		for (int ip = 0; ip < 3; ip++)
		{
			vertexData.position = tri[ip];
			vertexData.normal = norm;

			m_vertexBuffer.push_back(vertexData);
		}
	}
}

void MarchingCube::fillOctree(const std::vector<VertexData>& vertices)
{
	if (vertices.size() <= 0)
		return;
	size_t triangleCount = vertices.size() / 3;
	m_octreeMesh.initilize(DirectX::BoundingBox(float3(0.5f), float3(0.5f)), 3, 5, triangleCount);
	for (size_t i = 0; i < triangleCount; i++)
	{
		size_t idx = i * 3;
		Triangle tri;
		tri.points[0] = vertices[idx + 0];
		tri.points[1] = vertices[idx + 1];
		tri.points[2] = vertices[idx + 2];
		float3 pmin, pmax;
		pmin.x = min(tri.points[0].position.x, min(tri.points[1].position.x, tri.points[2].position.x));
		pmin.y = min(tri.points[0].position.y, min(tri.points[1].position.y, tri.points[2].position.y));
		pmin.z = min(tri.points[0].position.z, min(tri.points[1].position.z, tri.points[2].position.z));

		pmax.x = max(tri.points[0].position.x, max(tri.points[1].position.x, tri.points[2].position.x));
		pmax.y = max(tri.points[0].position.y, max(tri.points[1].position.y, tri.points[2].position.y));
		pmax.z = max(tri.points[0].position.z, max(tri.points[1].position.z, tri.points[2].position.z));
		DirectX::BoundingBox bb;
		DirectX::BoundingBox::CreateFromPoints(bb, pmin, pmax);
		m_octreeMesh.add(bb, tri, false);
	}
}

void MarchingCube::fillPipelineInstances()
{
	// Terrain
	m_pipelineInstance_terrain->setVertexBuffer(0, m_vertexBuffer);

	m_pipelineInstance_terrain->setConstantBuffer(0, getMatrixBuffer(), PipelineStage::Stage_Vertex);
	m_pipelineInstance_terrain->setVertexDrawCall((UINT)m_vertexBuffer.getBufferElementCapacity());

	// Shadow
	m_pipelineInstance_shadow->setVertexBuffer(0, m_vertexBuffer);
	m_pipelineInstance_shadow->setConstantBuffer(0, getMatrixBuffer(), PipelineStage::Stage_Vertex);
	m_pipelineInstance_shadow->setVertexDrawCall((UINT)m_vertexBuffer.getBufferElementCapacity());

	// Scanner
	m_pipelineInstance_scanning->setVertexBuffer(0, m_vertexBuffer);
	m_pipelineInstance_scanning->setConstantBuffer(0, getMatrixBuffer(), PipelineStage::Stage_Vertex);
	m_pipelineInstance_scanning->setVertexDrawCall((UINT)m_vertexBuffer.getBufferElementCapacity());
	//m_pipelineInstance_scanning->setConstantBuffer(1, m_cbuffer_scannerProperties, PipelineStage::Stage_Fragment);
}

void MarchingCube::bindColorBuffer(ConstantBuffer<TerrainColor>& cbuffer)
{
	m_pipelineInstance_terrain->setConstantBuffer(2, cbuffer, PipelineStage::Stage_Vertex);
}

void MarchingCube::_draw(const float4x4& matrix)
{
	DrawableObject::updateMatrixBuffer(matrix);
	if (m_vertexBuffer.getBufferElementCapacity() > 0) {
		// terrain
		Graphics::getInstance()->pushPipelineInstance(m_pipelineInstance_terrain, RenderingSection::OpaqueRendering);
		// Scanner
		if (m_drawScanner)
			Graphics::getInstance()->pushPipelineInstance(m_pipelineInstance_scanning, RenderingSection::PostDeferredRendering);
	}
}

std::vector<std::shared_ptr<PipelineInstanceBase>> MarchingCube::_getShadowInstances(const float4x4 matrix)
{
	std::vector<std::shared_ptr<PipelineInstanceBase>> instances;
	if (m_vertexBuffer.getBufferElementCapacity() > 0) {
		DrawableObject::updateMatrixBuffer(matrix);
		instances.push_back(m_pipelineInstance_shadow);
	}
	return instances;
}

MarchingCube::MarchingCube()
{
	m_sizeX = 0;
	m_sizeY = 0;
	m_sizeZ = 0;
	m_surfaceValue = 126.f;
	m_destroyValue = 255.f;
	m_startDataPos = int3(0, 0, 0);
	m_actor = nullptr;
	m_simulationActive = false;
}

MarchingCube::~MarchingCube()
{
}

void MarchingCube::runMarchingCubes(Physics& physics, const float4x4& matrix, const float3& scale)
{
	// clear former data
	static std::mutex mutex;
	m_vertexBuffer.clear();		// empty buffer so things can disapear when destroyed
	mutex.lock();
	physics.removeActor(m_actor);
	if (m_actor)
		m_actor->release();
	m_actor = nullptr;
	mutex.unlock();

	// generate new data
	for (int iz = 0; iz < m_sizeX; iz++)
	{
		for (int iy = 0; iy < m_sizeY; iy++)
		{
			for (int ix = 0; ix < m_sizeZ; ix++)
			{
				singleMarchCube(ix, iy, iz);
			}
		}
	}
	m_vertexBuffer.shrink_to_fit(); // shrink capacity to be equal list size (it is very important as vram data is created based on capacity)

	// fill octree
	fillOctree(m_vertexBuffer);

	// setup rendering
	m_vertexBuffer.updateBuffer();

	// Generate PhysX collider
	if (m_vertexBuffer.size() > 0) 
	{
		mutex.lock();
		m_actor = physics.generateTriangleMeshCollider(getVertexPositions(), float3::Transform(getPosition(), matrix), getScale() * scale, float3(5, 5, 0.1f));
		if (m_actor != nullptr)
		{
			m_actor->userData = nullptr;
			m_actor->setActorFlag(physx::PxActorFlag::eDISABLE_SIMULATION, true);
			m_simulationActive = false;
		}
		mutex.unlock();
	}
	fillPipelineInstances();
	m_vertexBuffer.clear();
}

void MarchingCube::runMarchingCubes()
{
	// Marching cube
	for (int iz = 0; iz < m_sizeX; iz++)
	{
		for (int iy = 0; iy < m_sizeY; iy++)
		{
			for (int ix = 0; ix < m_sizeZ; ix++)
			{
				singleMarchCube(ix, iy, iz);
			}
		}
	}

	m_vertexBuffer.clear();		// empty buffer so things can disapear when destroyed

	for (int iz = 0; iz < m_sizeZ; iz++)
	{
		for (int iy = 0; iy < m_sizeY; iy++)
		{
			for (int ix = 0; ix < m_sizeX; ix++)
			{
				singleMarchCube(ix, iy, iz);
			}
		}
	}
	m_vertexBuffer.shrink_to_fit(); // shrink capacity to be equal list size (it is very important as vram data is created based capacity)

	// fill octree
	fillOctree(m_vertexBuffer);

	// setup rendering
	m_vertexBuffer.updateBuffer();
	//m_vertexBuffer.clear(); // May not clear vertex data yet as it is needed to create physX collision data
	fillPipelineInstances();

	m_vertexBuffer.clear();
}

void MarchingCube::setStartDataPos(int3 pos)
{
	m_startDataPos = pos;
}

void MarchingCube::setTerrainData(std::shared_ptr<TERRAINDATATYPE[]> data)
{
	s_terrainData = data;
}

void MarchingCube::setNrCubes(int nr)
{
	s_nrCubes = nr;
}

void MarchingCube::setDataSizes(int x, int y, int z)
{
	m_sizeX = x;
	m_sizeY = y;
	m_sizeZ = z;
}

void MarchingCube::setDataSizes(int3 sizes)
{
	m_sizeX = sizes.x;
	m_sizeY = sizes.y;
	m_sizeZ = sizes.z;
}

void MarchingCube::setScannerState(bool state)
{
	m_drawScanner = state;
}

std::vector<float3> MarchingCube::getVertexPositions()
{
	std::vector<float3> vec;
	vec.resize(m_vertexBuffer.size());

	for (size_t i = 0; i < m_vertexBuffer.size(); i++)
	{
		vec[i] = m_vertexBuffer[i].position;
	}
	return vec;
}

int MarchingCube::getTriangleDataSize()
{
	return (int)m_vertexBuffer.getBufferElementCapacity();
}

void MarchingCube::setPhysicsActive(bool active)
{
	// only set flag when actor status changes, and only change status if this marhcing cube has an actor
	if (m_actor != nullptr && active != m_simulationActive)
	{
		m_simulationActive = active;
		m_actor->setActorFlag(physx::PxActorFlag::eDISABLE_SIMULATION, !active);
	}
}

void MarchingCube::clearVertexData()
{
	m_vertexBuffer.clear();
}

DirectX::BoundingBox MarchingCube::getLocalBoundingBox() const
{
	return DirectX::BoundingBox(float3(0.5f), float3(0.5f));
}

bool MarchingCube::raycast(float3 rayPosition, float3 rayDirection, float& distance, float3& intersectionPosition, float3& intersectionNormal, size_t& tests)
{
	if (rayDirection.Length() == 0 || distance == 0)
		return false;

	// Get matrices
	float4x4 mWorld = getMatrix();
	float4x4 mInvWorld = mWorld.Invert();
	float4x4 mInvTraWorld = mInvWorld.Transpose();
	// Transform to local space
	float3 lrayPos = float3::Transform(rayPosition, mInvWorld);
	float3 lrayPosDest = float3::Transform(rayPosition + rayDirection * distance, mInvWorld);
	float3 lrayDir = lrayPosDest - lrayPos;
	float lrayDistance = lrayDir.Length();
	lrayDir /= lrayDistance;
	// cull octree
	std::vector<Triangle*> triangles;
	float octreeDistance = lrayDistance;
	m_octreeMesh.cullElements(lrayPos, lrayDir, octreeDistance, triangles);
	tests += triangles.size();
	// triangle intersection tests
	float tmin = -1;
	size_t triIdx = 0;
	for (size_t i = 0; i < triangles.size(); i++)
	{
		float triDistance = lrayDistance;
		if (DirectX::TriangleTests::Intersects(lrayPos, lrayDir, triangles[i]->points[0].position, triangles[i]->points[1].position, triangles[i]->points[2].position, triDistance))
		{
			if (triDistance <= lrayDistance && (tmin == -1 || triDistance < tmin))
			{
				tmin = triDistance;
				triIdx = i;
			}
		}
	}
	// calculate result
	if (tmin != -1)
	{
		//hit
		float3 lPoint = lrayPos + lrayDir * tmin; // position
		float3 lNormal = triangles[triIdx]->calcFlatNormal(); // normal

		// position
		intersectionPosition = float3::Transform(lPoint, mWorld);
		// normal
		intersectionNormal = float3::TransformNormal(lNormal, mInvTraWorld);
		intersectionNormal.Normalize();
		// distance
		distance = (intersectionPosition - rayPosition).Length();
		return true;
	}
	else
		return false; // miss
}

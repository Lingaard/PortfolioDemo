#include "pch.h"
#include "MarchingCubeHandler.h"
#include "Profiler.h"
#include "ThreadPool.h"
#include "L_System.h"
#include "CaveCarver.h"
#include "Physics.h"
#include "Scene.h"
#include "Controls.h"
#include "AudioController.h"

void MarchingCubeHandler::initDataTexture(int sizeX, int sizeY, int sizeZ)
{
	// Round down to multiple of the amount of MarchingCubes. Don't want to handle exception cases at ends right now
	m_sizeX = sizeX - (sizeX % s_nrCubes);
	m_sizeY = sizeY - (sizeY % s_nrCubes);
	m_sizeZ = sizeZ - (sizeZ % s_nrCubes);
	m_totalSize = m_sizeX * m_sizeY * m_sizeZ;

	std::shared_ptr<TERRAINDATATYPE[]> sp(new TERRAINDATATYPE[m_totalSize]);
	m_terrainData.swap(sp);

	float longest = (float)max(max(m_sizeX, m_sizeY), m_sizeZ);
	rescale(float3(m_sizeX / longest, m_sizeY / longest, m_sizeZ / longest));
}

void MarchingCubeHandler::setTerrainPixel(int x, int y, int z, TERRAINDATATYPE value)
{
	int index = x + y * m_sizeX + z * m_sizeX * m_sizeY;
	if (0 <= index && index < m_totalSize) {
		m_terrainData[index] = value;
	}
}

TERRAINDATATYPE MarchingCubeHandler::getTerrainPixel(int x, int y, int z) const
{
	int index = x + y * m_sizeX + z * m_sizeX * m_sizeY;
	if (0 <= index && index < m_totalSize)
		return m_terrainData[index];
	else
		return 100;
}

TERRAINDATATYPE MarchingCubeHandler::getTerrainPixel(float3 pos) const
{
	int3 nodeIndex(pos.x, pos.y, pos.z);
	float3 frac(pos.x - nodeIndex.x, pos.y - nodeIndex.y, pos.z - nodeIndex.z);
	TERRAINDATATYPE edgeValue[2][2][2];
	for (int ix = 0; ix < 2; ix++)
		for (int iy = 0; iy < 2; iy++)
			for (int iz = 0; iz < 2; iz++)
				edgeValue[ix][iy][iz] = getTerrainPixel(nodeIndex.x + ix, nodeIndex.y + iy, nodeIndex.z + iz);

	// bottom plane
	TERRAINDATATYPE botx0 = Lerp(edgeValue[0][0][0], edgeValue[1][0][0], frac.x);
	TERRAINDATATYPE botx1 = Lerp(edgeValue[0][0][1], edgeValue[1][0][1], frac.x);
	TERRAINDATATYPE botz = Lerp(botx0, botx1, frac.z);
	// top plane
	TERRAINDATATYPE topx0 = Lerp(edgeValue[0][1][0], edgeValue[1][1][0], frac.x);
	TERRAINDATATYPE topx1 = Lerp(edgeValue[0][1][1], edgeValue[1][1][1], frac.x);
	TERRAINDATATYPE topz = Lerp(topx0, topx1, frac.z);
	// center
	TERRAINDATATYPE midy = Lerp(botz, topz, frac.y);
	return midy;
}

float3 MarchingCubeHandler::translateWorldToDataSpace(float3 worldPos) const
{
	float3 pos = translateWorldToLocalSpace(worldPos);
	// localSpace to dataSpace [0, m_sizeX/Y/Z]
	float3 dataSizes((float)m_sizeX, (float)m_sizeY, (float)m_sizeZ);
	pos *= dataSizes;
	return pos;
}

float3 MarchingCubeHandler::translateWorldToLocalSpace(float3 worldPos) const
{
	// translate worldPos to MarchingCubeHandler's local position. 
	// worldSpace [-inf, inf] to localSpace [0, 1]
	float4x4 invWorldMat = (getScalingMatrix() * getRotationMatrix() * getTranslateMatrix()).Invert();
	return float3::Transform(worldPos, invWorldMat);
}

bool MarchingCubeHandler::queueMarchingCube(int3 cubeIdx)
{
	if (cubeIdx.x >= 0 && cubeIdx.x < s_nrCubes &&
		cubeIdx.y >= 0 && cubeIdx.y < s_nrCubes &&
		cubeIdx.z >= 0 && cubeIdx.z < s_nrCubes) {
		const int linearIdx = cubeIdx.x + cubeIdx.y * s_nrCubes + cubeIdx.z * s_nrCubes * s_nrCubes;
		if (linearIdx < 0 && linearIdx >= m_marchingCubeQueueLookup.size())
			return false; // idx outside valid value
		if (!m_marchingCubeQueueLookup[linearIdx]) {
			m_marchingCubeQueueLookup[linearIdx] = true;
			m_marchingCubeQueue.push_back(cubeIdx);
			return true;
		}
	}
	return false;
}

bool MarchingCubeHandler::queueMarchingCube_pixelIndex(int3 pixelIdx)
{
	const int3 dataStride(m_sizeX / s_nrCubes, m_sizeY / s_nrCubes, m_sizeZ / s_nrCubes);
	int3 cubeIdx = pixelIdx / dataStride;
	int3 restIdx = int3(pixelIdx.x % dataStride.x, pixelIdx.y % dataStride.y, pixelIdx.z % dataStride.z);
	// add to queue
	bool addedAnyCubeToQueue = queueMarchingCube(cubeIdx);
	// check if adjacent
	if (restIdx.x == 0)
		addedAnyCubeToQueue |= queueMarchingCube(cubeIdx + int3(-1, 0, 0));
	if (restIdx.y == 0)
		addedAnyCubeToQueue |= queueMarchingCube(cubeIdx + int3(0, -1, 0));
	if (restIdx.z == 0)
		addedAnyCubeToQueue |= queueMarchingCube(cubeIdx + int3(0, 0, -1));
	if (restIdx.x == m_sizeX - 1)
		addedAnyCubeToQueue |= queueMarchingCube(cubeIdx + int3(1, 0, 0));
	if (restIdx.y == m_sizeY - 1)
		addedAnyCubeToQueue |= queueMarchingCube(cubeIdx + int3(0, 1, 0));
	if (restIdx.z == m_sizeZ - 1)
		addedAnyCubeToQueue |= queueMarchingCube(cubeIdx + int3(0, 0, 1));

	return addedAnyCubeToQueue;
}

void MarchingCubeHandler::_draw(const float4x4& matrix)
{
	// fetch frustum planes
	Camera& camera = Graphics::getInstance()->getActiveCamera();
	FrustumPlanes fp(camera.getBoundingFrustum());
	// transform to local space
	fp.Transform(getMatrix().Invert()); // transform frustum planes to local space
	// cull octree
	std::vector<MarchingCube**> cubes;
	Profiler::start("MC Culling");
	m_octree->cullElements(fp, cubes);
	Profiler::stop();
	// draw chunks
	bool scannerActive = (m_scanningState != Scan_Inactive);
	Profiler::start("MC Draw");
	for (size_t i = 0; i < cubes.size(); i++)
	{
		(*cubes[i])->setScannerState(scannerActive);
		(*cubes[i])->draw(matrix);
	}
	Profiler::stop();

	// draw shadow
	std::shared_ptr<DrawableOctree<DrawableObject*>>* request = (std::shared_ptr<DrawableOctree<DrawableObject*>>*) & m_octree;
	request->get()->m_matrix_renderingOrientation = matrix;
	Graphics::getInstance()->pushDrawableOctree(*request);

	// draw decor
	for (size_t iColl = 0; iColl < m_decor.size(); iColl++)
	{
		const DecorCollection& collection = m_decor[iColl];
		for (size_t iInstance = 0; iInstance < collection.m_instances.size(); iInstance++)
		{
			Graphics::getInstance()->debugDraw_model(collection.m_name, collection.m_instances[iInstance].matrix, collection.m_instances[iInstance].color);
		}
	}

	//float3 cellSize = float3(1.f) / float3(m_sizeX, m_sizeY, m_sizeZ);
	//float3 localPlayerPos = float3::Transform(getScene()->m_player.getPosition(), matrix.Invert());
	//size_t count = 0;
	//for (size_t ix = 0; ix < m_sizeX; ix++)
	//{
	//	for (size_t iy = 0; iy < m_sizeY; iy++)
	//	{
	//		for (size_t iz = 0; iz < m_sizeZ; iz++)
	//		{
	//			Transformation transform;
	//			transform.setPosition(float3((float)ix, (float)iy, (float)iz) * cellSize);
	//			transform.setScale(0.001f);
	//			if ((transform.getPosition() - localPlayerPos).Length() < 0.1f) {
	//				Graphics::getInstance()->debugDraw_model("sphere.fbx", transform.getMatrix() * matrix, (getTerrainPixel(ix, iy, iz) < m_surfaceValue ? float3(1, 0, 0) : float3(0, 1, 0)));
	//				count++;
	//			}
	//		}
	//	}
	//}
	//ErrorLogger::log("Cells: " + std::to_string(count) + "/" + std::to_string(m_sizeX * m_sizeY * m_sizeZ));
}

MarchingCubeHandler::MarchingCubeHandler() :
	GameObject(Identifier::GetClassID<MarchingCubeHandler>(), PhysicsType::Static),
	m_cbuffer_scannerProperties(Graphics::getInstance()->getScanningPropertiesBuffer())
{
	ObjectNode::setNodeName("MarchingCube");

	m_sizeX = 0;
	m_sizeY = 0;
	m_sizeZ = 0;
	m_totalSize = 0;

	m_surfaceValue = 126.f;
	m_destroyValue = 255.f;
}

MarchingCubeHandler::~MarchingCubeHandler()
{

}

void MarchingCubeHandler::imgui_edit()
{
	if (ImGui::BeginTabBar("TabBar")) {
		if (ImGui::BeginTabItem("Transform")) {
			ObjectNode::imgui_properties();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Cubes")) {
			ImGui::InputInt("Seed", &editInfo.seed);
			ImGui::SameLine();
			if (ImGui::Button("Random"))
				editInfo.seed = rand();
			ImGui::InputInt("Size", &editInfo.size);
			ImGui::InputFloat("Scale", &editInfo.scale);
			if (ImGui::Button("Init")) {
				init(editInfo.size, editInfo.size, editInfo.size, editInfo.scale);
			}
			ImGui::SameLine();
			if (ImGui::Button("Generate")) {
				generateData_testCave(2, editInfo.seed);
				runAllMarchingCubes();
			}
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

void MarchingCubeHandler::init(int sizeX, int sizeY, int sizeZ, float scale)
{
	setScale(float3(1.f) * scale);
	initDataTexture(sizeX, sizeY, sizeZ);
	m_cbuffer_terrainColor.init();

	initTerrainColorData();
	initCubes();
	initOctree();
}

void MarchingCubeHandler::initTerrainColorData()
{
	m_terrainColorData.colorFloor[0] = float4(171.f / 255.f, 148.f / 255.f, 122.f / 255.f, 1);
	m_terrainColorData.colorWall[0] = float4(150.f / 255.f, 108.f / 255.f, 100.f / 255.f, 1);
	m_terrainColorData.colorCeil[0] = float4(98.f / 255.f, 85.f / 255.f, 101.f / 255.f, 1);
	static float bonusColor[3][4] = { 251.f / 255.f, 185.f / 255.f, 84.f / 255.f, 1,
									  190.f / 255.f, 173.f / 255.f, 224.f / 255.f, 1,
									  240.f / 255.f, 79.f / 255.f, 120.f / 255.f, 1 };
	static float interpValue = 0.75f;

	float4 colorFloat4[3];
	for (size_t i = 0; i < 3; i++)
	{
		colorFloat4[i] = float4(bonusColor[i][0], bonusColor[i][1], bonusColor[i][2], 1);
	}

	for (size_t i = 0; i < 3; i++)
	{
		m_terrainColorData.colorFloor[i + 1] = m_terrainColorData.colorFloor[0] * Lerp(float4(1, 1, 1, 1), colorFloat4[i], interpValue);
		m_terrainColorData.colorWall[i + 1] = m_terrainColorData.colorWall[0] * Lerp(float4(1, 1, 1, 1), colorFloat4[i], interpValue);
		m_terrainColorData.colorCeil[i + 1] = m_terrainColorData.colorCeil[0] * Lerp(float4(1, 1, 1, 1), colorFloat4[i], interpValue);
	}
	m_terrainColorData.colorLimits = float2(0.663f, 0.461f);
	m_terrainColorData.terrainHightInverted = 1 / getScale().y;

	m_cbuffer_terrainColor.set(m_terrainColorData);
	m_cbuffer_terrainColor.updateBuffer();

}

void MarchingCubeHandler::_update(double dt)
{
	if (!isLinked())
		return;

	//initTerrainColorData();
	// Scanner imgui properties
	/*if (ImGui::Begin("Scanner Properties")) {
		ImGui::PushItemWidth(100);
		ImGui::InputFloat("Time", &m_scannerAnimationLength);
		ImGui::InputFloat("Animation Power", &m_scannerAnimationPower);
		ImGui::InputFloat("Forward Falloff Distance", &m_scannerForwardFalloffDistance);
		ImGui::InputFloat("Range", &m_scannerMaxDistance);
		ImGui::SliderFloat("Transparency Falloff Threshold", &m_scannerTransparencyFalloffThreshold, 0, 1);
		ImGui::ColorEdit3("Color Front", (float*)&m_cbuffer_scannerProperties.colorFront);
		ImGui::ColorEdit3("Color Side", (float*)&m_cbuffer_scannerProperties.colorSide);
		ImGui::PopItemWidth();
	}
	ImGui::End();*/

	static bool changed = true;

#if DEV_MODE
	// TerrainColor properties
	/*if (ImGui::Begin("Terrrain color properties"))
	{
		if (ImGui::SliderFloat("dot value floor", &m_terrainColorData.colorLimits.x, -1, 1))
			changed = true;
		if (ImGui::SliderFloat("dot value ceiling", &m_terrainColorData.colorLimits.y, -1, 1))
			changed = true;
		if (ImGui::ColorEdit3("Color Floor", (float*)&m_terrainColorData.colorFloor))
			changed = true;
		if (ImGui::ColorEdit3("Color Walls", (float*)&m_terrainColorData.colorWall))
			changed = true;
		if (ImGui::ColorEdit3("Color Ceiling", (float*)&m_terrainColorData.colorCeil))
			changed = true;
	}
	ImGui::End();*/
#endif

	//if (changed)
	//{
	//	changed = false;
	//	m_cbuffer_terrainColor.set(m_terrainColorData);
	//	m_cbuffer_terrainColor.updateBuffer();

	//	for (size_t x = 0; x < s_nrCubes; x++)
	//	{
	//		for (size_t y = 0; y < s_nrCubes; y++)
	//		{
	//			for (size_t z = 0; z < s_nrCubes; z++)
	//			{
	//				m_mcs[x][y][z].bindColorBuffer(m_cbuffer_terrainColor);
	//			}
	//		}
	//	}
	//}

	relayColorToGraphics();

	switch (m_scanningState)
	{
	case MarchingCubeHandler::Scan_Inactive:
		m_scannerCooldown = Clamp<float>(m_scannerCooldown - (float)dt, 0, m_scannerMaxCooldown);
		if (Controls::controlDown(Controls::ControlFunction::SCAN) && !m_waitForCooldown)
		{
			m_scanningState = Scan_Active;
			// On Activation
			m_cbuffer_scannerProperties.transparency = 1;
			m_scannerAnimationTimer = 0;
			m_scannerActivationTimer = 0;
			AudioController::getInstance()->quickPlay("scan");
		}
		break;
	case MarchingCubeHandler::Scan_Active:
		if (!m_scannerUnlimitedCooldown)
			m_scannerCooldown = Clamp<float>(m_scannerCooldown + (float)dt, 0, m_scannerMaxCooldown);
		m_scannerActivationTimer = Clamp<float>(m_scannerActivationTimer + (float)dt, 0, m_scannerMinimumActivationTime);
		if ((!Controls::controlDown(Controls::ControlFunction::SCAN) && m_scannerActivationTimer == m_scannerMinimumActivationTime) || m_waitForCooldown)
		{
			m_scanningState = Scan_Fade;
			// On Fade
			m_scannerAlphaTimer = m_scannerAlphaFadeTime;
		}
		break;
	case MarchingCubeHandler::Scan_Fade:
		m_scannerCooldown = Clamp<float>(m_scannerCooldown - (float)dt, 0, m_scannerMaxCooldown);
		if (m_scannerAlphaTimer == 0)
		{
			m_scanningState = Scan_Inactive;
			// On Inactive
			m_scannerAnimationTimer = 0;
		}
		break;
	}
	if (!m_waitForCooldown && m_scannerCooldown == m_scannerMaxCooldown) // on begin cooldown
	{
		m_waitForCooldown = true;
	}
	if (m_waitForCooldown && m_scannerCooldown == 0) // on end cooldown
	{
		m_waitForCooldown = false;
	}

	if (m_scanningState == Scan_Active || m_scanningState == Scan_Fade)
	{
		// Scanner Animation
		m_scannerAnimationTimer = Clamp<float>(m_scannerAnimationTimer + (float)dt / m_scannerAnimationLength, 0, 1); // update timer
		float scanAnimation = 1 - pow(1 - sin(m_scannerAnimationTimer * DirectX::XM_PI / 2.f), m_scannerAnimationPower);
		m_cbuffer_scannerProperties.radiusMid = scanAnimation * m_scannerMaxDistance;
		m_cbuffer_scannerProperties.radiusMax = m_cbuffer_scannerProperties.radiusMid + m_scannerForwardFalloffDistance;
		m_cbuffer_scannerProperties.radiusMin = 0;
	}
	if (m_scanningState == Scan_Fade)
	{
		m_scannerAlphaTimer = max(m_scannerAlphaTimer - (float)dt, 0);
		float alpha = m_scannerAlphaTimer / m_scannerAlphaFadeTime;
		alpha = Clamp<float>(alpha, 0, 1);
		m_cbuffer_scannerProperties.transparency = alpha;
	}
	m_cbuffer_scannerProperties.timer += (float)dt;
}

void MarchingCubeHandler::read_sceneNode_internal(std::ifstream& file)
{
	int sizeX, sizeY, sizeZ;
	fileRead(file, sizeX);
	fileRead(file, sizeY);
	fileRead(file, sizeZ);
	float scale = fileRead<float>(file);
	unsigned int seed = fileRead<unsigned int>(file);

	if (sizeX > 0 && sizeY > 0 && sizeZ > 0) {
		init(sizeX, sizeY, sizeZ, scale);
		generateData_testCave(2, seed);
		runAllMarchingCubes();
	}

	editInfo.size = sizeX;
	editInfo.scale = scale;
	editInfo.seed = seed;
}

void MarchingCubeHandler::write_sceneNode_internal(std::ofstream& file)
{
	fileWrite(file, m_sizeX);
	fileWrite(file, m_sizeY);
	fileWrite(file, m_sizeZ);
	fileWrite(file, getScale().x);
	fileWrite(file, m_latestSeed);
}

float easeInCubic(float x)
{
	return x * x;
}
float3 MarchingCubeHandler::getTerrainColorFromLocalPosition(float3 localPosition, float3 normal)
{
	float4 retCol4;
	const TerrainColor& data = m_cbuffer_terrainColor;

	// set color while position and normal is in worldSpace
	int colorIndex = Clamp<int>((int)(3 - floor(localPosition.y * 4)), 0, 3);
	float angle = normal.Dot(float3::Up);
	if (angle > data.colorLimits.x)
		retCol4 = data.colorFloor[colorIndex];
	else if (angle > data.colorLimits.y)
	{
		float inter = (angle - data.colorLimits.y) / (data.colorLimits.x - data.colorLimits.y);
		if (inter < 0.5f)
			retCol4 = Lerp<float4>(data.colorCeil[colorIndex], data.colorWall[colorIndex], Clamp<float>(easeInCubic(inter * 2), 0, 1));
		else
			retCol4 = Lerp<float4>(data.colorWall[colorIndex], data.colorFloor[colorIndex], Clamp<float>(easeInCubic(inter * 2 - 1), 0, 1));
	}
	else
		retCol4 = data.colorCeil[colorIndex];

	return float3(retCol4.x, retCol4.y, retCol4.z);
}

void MarchingCubeHandler::eraseDecor_sphere(float3 worldPos, float radius)
{
	float radiusSquared = radius * radius;
	for (size_t iColl = 0; iColl < m_decor.size(); iColl++)
	{
		DecorCollection& collection = m_decor[iColl];
		for (size_t iInstance = 0; iInstance < collection.m_instances.size(); iInstance++)
		{
			float3 instanceWPos = float3::Transform(float3::Zero, collection.m_instances[iInstance].matrix);
			if ((instanceWPos - worldPos).LengthSquared() < radiusSquared) {
				collection.m_instances.erase(collection.m_instances.begin() + iInstance);
				iInstance--;
			}
		}
	}
}

float3 MarchingCubeHandler::getDataFieldFlow(float3 worldPos, float localGridStepSize)
{
	// based on central difference ( df(x) = f(x+h)-f(x-h) )
	// flow points to more air
	float h = localGridStepSize;
	float3 cellId = translateWorldToDataSpace(worldPos);
	float3 flow;
	flow.x = (float)((int)getTerrainPixel(cellId + float3(h, 0, 0)) - (int)getTerrainPixel(cellId - float3(h, 0, 0))) / 255;
	flow.y = (float)((int)getTerrainPixel(cellId + float3(0, h, 0)) - (int)getTerrainPixel(cellId - float3(0, h, 0))) / 255;
	flow.z = (float)((int)getTerrainPixel(cellId + float3(0, 0, h)) - (int)getTerrainPixel(cellId - float3(0, 0, h))) / 255;
	return flow;
}

float3 MarchingCubeHandler::getDataFieldFlow(int3 pixelIdx, float localGridStepSize)
{
	// based on central difference ( df(x) = f(x+h)-f(x-h) )
	// flow points to more air
	float h = localGridStepSize;
	float3 cellId = float3((float)pixelIdx.x, (float)pixelIdx.y, (float)pixelIdx.z);
	float3 flow;
	flow.x = (float)((int)getTerrainPixel(cellId + float3(h, 0, 0)) - (int)getTerrainPixel(cellId - float3(h, 0, 0))) / 255;
	flow.y = (float)((int)getTerrainPixel(cellId + float3(0, h, 0)) - (int)getTerrainPixel(cellId - float3(0, h, 0))) / 255;
	flow.z = (float)((int)getTerrainPixel(cellId + float3(0, 0, h)) - (int)getTerrainPixel(cellId - float3(0, 0, h))) / 255;
	return flow;
}

void MarchingCubeHandler::visualizeDataField()
{
	float3 camPos = Graphics::getInstance()->getActiveCamera().getPosition();
	float3 camForward = Graphics::getInstance()->getActiveCamera().getForward();
	float3 pos;
	float3 camToPos;
	float3 flow;
	Transformation t;
	static float filterValue = 1;
	static float fov = 0.8f;
	static bool enable = false;
	static float offset = 1.f;
	if (ImGui::Begin("visData"))
	{
		ImGui::SliderFloat("filter", &filterValue, 0, 2);
		ImGui::SliderFloat("fov", &fov, 0.1f, 1);
		ImGui::SliderFloat("offset", &offset, 0.5f, 5);
		ImGui::Checkbox("enable", &enable);
	}
	ImGui::End();
	if (!enable)
		return;
	for (int iz = 0; iz < m_sizeZ; iz++)
	{
		pos.z = getPosition().z + iz * getScale().z / m_sizeZ;
		for (int iy = 0; iy < m_sizeY; iy++)
		{
			pos.y = getPosition().y + iy * getScale().y / m_sizeY;
			for (int ix = 0; ix < m_sizeX; ix++)
			{
				pos.x = getPosition().x + ix * getScale().x / m_sizeX;
				camToPos = pos - camPos;
				camToPos.Normalize();
				if (camForward.Dot(camToPos) > fov)
				{
					t.setPosition(pos);
					float3 flow = getDataFieldFlow(pos, offset);
					t.lookTo(flow);
					float3 color = float3(1) * flow.Length();
					if (flow.Length() > filterValue)
						Graphics::getInstance()->debugDraw_model("cube.fbx", pos, t.getRotation(), float3(0.01f, 0.01f, 0.05f), color);
				}
			}
		}
	}
}

void MarchingCubeHandler::initOctree()
{
	float3 worldStride(float3(1, 1, 1) / s_nrCubes); // local chunk size
	size_t cap = (size_t)pow(s_nrCubes, 3); // element count
	int branching = max((int)floor(log2(s_nrCubes)) - 1, 1); // optimal branching steps
	m_octree->initilize(DirectX::BoundingBox(float3(0.5f), float3(0.5f)), branching, 1, cap);
	for (size_t x = 0; x < s_nrCubes; x++)
	{
		for (size_t y = 0; y < s_nrCubes; y++)
		{
			for (size_t z = 0; z < s_nrCubes; z++)
			{
				if (m_mcs[x][y][z].getTriangleDataSize() <= 0)
					continue;
				float3 pos = float3((float)x, (float)y, (float)z) * worldStride;
				float3 size = worldStride;
				DirectX::BoundingBox bb(pos + size * 0.5f, size * 0.5f);
				m_octree->add(bb, &m_mcs[x][y][z], false);
			}
		}
	}
}

struct CubeIntersection {
	MarchingCube* cube = nullptr;
	float distance = 0;
};
/* function for sorting with qsort */
int compareCubeIntersections(const void* a, const void* b)
{
	float va = (*(CubeIntersection*)a).distance;
	float vb = (*(CubeIntersection*)b).distance;
	if (va < vb)
		return -1;
	else if (va > vb)
		return 1;
	return 0;
}
bool MarchingCubeHandler::raycast(float3 rayPosition, float3 rayDirection, float& distance, float3& intersectionPosition, float3& intersectionNormal)
{
	if (rayDirection.Length() < 0.00001f || distance < 0.00001f)
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

	float3 lPoint, lNormal;
	if (longRaycast_localSpace(lrayPos, lrayDir, lrayDistance, lPoint, lNormal)) {
		// position
		intersectionPosition = float3::Transform(lPoint, mWorld);
		// normal
		intersectionNormal = float3::TransformNormal(lNormal, mInvTraWorld);
		intersectionNormal.Normalize();
		// distance
		distance = (intersectionPosition - rayPosition).Length();
		return true;
	}
	return false;
}

bool MarchingCubeHandler::longRaycast_localSpace(float3 rayPosition, float3 rayDirection, float& distance, float3& intersectionPosition, float3& intersectionNormal)
{
	if (rayDirection.Length() < 0.00001f || distance < 0.00001f)
		return false;

	// cull octree
	std::vector<MarchingCube**> cubes;
	float octreeDistance = distance;
	m_octree->cullElements(rayPosition, rayDirection, octreeDistance, cubes);
	m_rayInfo.totalCubes = (size_t)pow(s_nrCubes, 3);
	m_rayInfo.culledCubes = cubes.size();

	// check intersected cubes
	std::vector<CubeIntersection> intersectedCubes;
	intersectedCubes.reserve(cubes.size());
	for (size_t i = 0; i < cubes.size(); i++)
	{
		float cubeDistance = 0;
		DirectX::BoundingBox cubeBB = (*cubes[i])->getBoundingBox();
		if (cubeBB.Contains(rayPosition) || (cubeBB.Intersects(rayPosition, rayDirection, cubeDistance) && cubeDistance < distance))
		{
			if (cubeDistance <= distance)
			{
				CubeIntersection cubeTest;
				cubeTest.cube = *cubes[i];
				cubeTest.distance = cubeDistance;
				intersectedCubes.push_back(cubeTest);
			}
		}
	}
	// sort intersected cubes
	qsort(intersectedCubes.data(), intersectedCubes.size(), sizeof(CubeIntersection), compareCubeIntersections);
	m_rayInfo.rayIntersectedCubes = intersectedCubes.size();

	// raycast against sorted cubes
	size_t triTests = 0;
	size_t triTotal = 0;
	float tmin = -1;
	float3 lPoint, lNormal;
	int iterations = 0;
	for (size_t i = 0; i < intersectedCubes.size(); i++)
	{
		iterations++;
		triTotal += intersectedCubes[i].cube->getTriangleDataSize() / 3;
		MarchingCube* cube = intersectedCubes[i].cube;
		float lrayCubeDistance = distance;
		float3 cubeIntersectionPoint, cubeIntersectionNormal;
		if (cube->raycast(rayPosition, rayDirection, lrayCubeDistance, cubeIntersectionPoint, cubeIntersectionNormal, triTests))
		{
			// hit
			if (tmin == -1 || lrayCubeDistance < tmin)
			{
				tmin = lrayCubeDistance;
				lPoint = cubeIntersectionPoint;
				lNormal = cubeIntersectionNormal;
				break; // can break as this is the closest triangle
			}
		}
	}
	m_rayInfo.loopIterations = iterations;
	m_rayInfo.totalTriangles = triTotal;
	m_rayInfo.culledTriangles = triTests;

	// calculate result
	if (tmin != -1)
	{
		// position
		intersectionPosition = lPoint;
		// normal
		intersectionNormal = lNormal;
		// distance
		distance = (intersectionPosition - rayPosition).Length();
		return true; // hit 
	}
	else
		return false; // miss
}

bool MarchingCubeHandler::shortRaycast(float3 rayPosition, float3 rayDirection, float& distance, float3& intersectionPosition, float3& intersectionNormal)
{
	if (rayDirection.Length() < 0.00001f || distance < 0.00001f)
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

	float3 lPoint, lNormal;
	if (shortRaycast_localSpace(lrayPos, lrayDir, lrayDistance, lPoint, lNormal)) {
		// position
		intersectionPosition = float3::Transform(lPoint, mWorld);
		// normal
		intersectionNormal = float3::TransformNormal(lNormal, mInvTraWorld);
		intersectionNormal.Normalize();
		// distance
		distance = (intersectionPosition - rayPosition).Length();
		return true;
	}
	return false;
}

bool MarchingCubeHandler::shortRaycast_localSpace(float3 rayPosition, float3 rayDirection, float& distance, float3& intersectionPosition, float3& intersectionNormal)
{
	if (rayDirection.Length() < 0.00001f || distance < 0.00001f)
		return false;

	float3 rayDestination = rayPosition + rayDirection * distance;

	// find chunks
	float3 cubeSize = float3(1.f) / s_nrCubes;
	float3 cube1IdxF = rayPosition / cubeSize;
	float3 cube2IdxF = rayDestination / cubeSize;
	int3 cube1Idx((int)cube1IdxF.x, (int)cube1IdxF.y, (int)cube1IdxF.z);
	int3 cube2Idx((int)cube2IdxF.x, (int)cube2IdxF.y, (int)cube2IdxF.z);
	int3 cubeMinIdx(min(cube1Idx.x, cube2Idx.x), min(cube1Idx.y, cube2Idx.y), min(cube1Idx.z, cube2Idx.z));
	int3 cubeMaxIdx(max(cube1Idx.x, cube2Idx.x), max(cube1Idx.y, cube2Idx.y), max(cube1Idx.z, cube2Idx.z));
	cubeMinIdx = int3(Clamp(cubeMinIdx.x, 0, s_nrCubes - 1), Clamp(cubeMinIdx.y, 0, s_nrCubes - 1), Clamp(cubeMinIdx.z, 0, s_nrCubes - 1));
	cubeMaxIdx = int3(Clamp(cubeMaxIdx.x, 0, s_nrCubes - 1), Clamp(cubeMaxIdx.y, 0, s_nrCubes - 1), Clamp(cubeMaxIdx.z, 0, s_nrCubes - 1));

	// Ray cast chunks
	size_t triTests = 0;
	float tmin = -1;
	float3 lPoint, lNormal;
	for (size_t x = cubeMinIdx.x; x <= cubeMaxIdx.x; x++)
	{
		for (size_t y = cubeMinIdx.y; y <= cubeMaxIdx.y; y++)
		{
			for (size_t z = cubeMinIdx.z; z <= cubeMaxIdx.z; z++)
			{
				MarchingCube* cube = &m_mcs[x][y][z];
				float lrayCubeDistance = distance;
				float3 cubeIntersectionPoint, cubeIntersectionNormal;
				if (cube->raycast(rayPosition, rayDirection, lrayCubeDistance, cubeIntersectionPoint, cubeIntersectionNormal, triTests))
				{
					// hit
					if (tmin == -1 || lrayCubeDistance < tmin)
					{
						tmin = lrayCubeDistance;
						lPoint = cubeIntersectionPoint;
						lNormal = cubeIntersectionNormal;
					}
				}
			}
		}
	}

	// calculate result
	if (tmin != -1)
	{
		// position
		intersectionPosition = lPoint;
		// normal
		intersectionNormal = lNormal;
		// distance
		distance = (intersectionPosition - rayPosition).Length();
		return true; // hit 
	}
	else
		return false; // miss
}

bool MarchingCubeHandler::raycast_localSpace(float3 rayPosition, float3 rayDirection, float& distance, float3& intersectionPosition, float3& intersectionNormal)
{
	if (rayDirection.Length() < 0.00001f || distance < 0.00001f)
		return false;

	if (distance <= 1.f / s_nrCubes)
		return shortRaycast_localSpace(rayPosition, rayDirection, distance, intersectionPosition, intersectionNormal);
	else
		return longRaycast_localSpace(rayPosition, rayDirection, distance, intersectionPosition, intersectionNormal);
}

void MarchingCubeHandler::initCubes()
{
	int3 dataStride(m_sizeX / s_nrCubes, m_sizeY / s_nrCubes, m_sizeZ / s_nrCubes);
	int3 startDataPos;

	float3 worldStride(float3(1, 1, 1) / s_nrCubes);

	// set static members
	MarchingCube::setNrCubes(s_nrCubes);
	MarchingCube::setTerrainData(m_terrainData);

	// init cubes
	for (int z = 0; z < s_nrCubes; z++)
	{
		startDataPos.z = z * dataStride.z;
		for (int y = 0; y < s_nrCubes; y++)
		{
			startDataPos.y = y * dataStride.y;
			for (int x = 0; x < s_nrCubes; x++)
			{
				startDataPos.x = x * dataStride.x;

				m_mcs[x][y][z].setStartDataPos(startDataPos);
				m_mcs[x][y][z].move(float3((float)x, (float)y, (float)z) * worldStride);
				m_mcs[x][y][z].setScale(worldStride);
				m_mcs[x][y][z].setDataSizes(dataStride);

				m_mcs[x][y][z].bindColorBuffer(m_cbuffer_terrainColor);
			}
		}
	}
}

void MarchingCubeHandler::runAllMarchingCubes(Physics& physics)
{
	Profiler::start("RunAllMarchingCubes");

	// create mesh
	ThreadPool* tp = ThreadPool::getInstance();
	for (int z = 0; z < s_nrCubes; z++)
	{
		tp->queue([this, z, &physics] {
			for (int y = 0; y < s_nrCubes; y++)
			{
				for (int x = 0; x < s_nrCubes; x++)
				{
					m_mcs[x][y][z].runMarchingCubes(physics, getMatrix(), getScale());
				}
			}
			});
	}
	tp->WaitForAll();
	m_marchingCubeQueue.clear();
	m_marchingCubeQueueLookup.reset();

	initOctree();

	Profiler::stop();
}

void MarchingCubeHandler::runQueuedMarchingCubes(Physics& physics)
{
	Profiler::start("RunQueuedMarchingCubes");

	ThreadPool* tp = ThreadPool::getInstance();
	bool anyTerrainUpdates = (m_marchingCubeQueue.size() > 0);
	if (anyTerrainUpdates) {
		for (size_t i = 0; i < m_marchingCubeQueue.size(); i++)
		{
			int3 id = m_marchingCubeQueue[i];
			// update terrain mesh
			tp->queue([this, id, &physics] {
				m_mcs[id.x][id.y][id.z].runMarchingCubes(physics, getMatrix(), getScale());
				});
		}
		tp->WaitForAll();
		m_marchingCubeQueue.clear();
		m_marchingCubeQueueLookup.reset();

		initOctree();
	}

	Profiler::stop();
}

void MarchingCubeHandler::runAllMarchingCubes()
{
	Profiler::start("RunAllMarchingCubes_without_physics");

	// create mesh
	ThreadPool* tp = ThreadPool::getInstance();
	for (int z = 0; z < s_nrCubes; z++)
	{
		tp->queue([this, z] {
			for (int y = 0; y < s_nrCubes; y++)
			{
				for (int x = 0; x < s_nrCubes; x++)
				{
					m_mcs[x][y][z].runMarchingCubes();
				}
			}
			});
	}
	tp->WaitForAll();
	m_marchingCubeQueue.clear();
	m_marchingCubeQueueLookup.reset();

	initOctree();

	Profiler::stop();
}

void MarchingCubeHandler::runQueuedMarchingCubes()
{
	Profiler::start("RunQueuedMarchingCubes");

	ThreadPool* tp = ThreadPool::getInstance();
	bool anyTerrainUpdates = (m_marchingCubeQueue.size() > 0);
	if (anyTerrainUpdates) {
		for (size_t i = 0; i < m_marchingCubeQueue.size(); i++)
		{
			int3 id = m_marchingCubeQueue[i];
			// update terrain mesh
			tp->queue([this, id] {
				m_mcs[id.x][id.y][id.z].runMarchingCubes();
				});
		}
		tp->WaitForAll();
		m_marchingCubeQueue.clear();
		m_marchingCubeQueueLookup.reset();

		initOctree();
	}

	Profiler::stop();
	//if (m_marchingCubeQueue.size() > 0)
		//std::cout << "terrain is " << getTriangleMeshSize() / 1000000.f << "M bytes\n";
}

void MarchingCubeHandler::setTerrainData(int sizeX, int sizeY, int sizeZ, TERRAINDATATYPE arr[])
{
	m_sizeX = sizeX;
	m_sizeY = sizeY;
	m_sizeZ = sizeZ;
	m_totalSize = m_sizeX * m_sizeY * m_sizeZ;

	std::shared_ptr<TERRAINDATATYPE[]> sp(arr);
	m_terrainData.swap(sp);
}

void MarchingCubeHandler::setTerrainData(int sizeX, int sizeY, int sizeZ, std::shared_ptr<TERRAINDATATYPE[]> sp)
{
	m_sizeX = sizeX;
	m_sizeY = sizeY;
	m_sizeZ = sizeZ;
	m_totalSize = m_sizeX * m_sizeY * m_sizeZ;
	m_terrainData = sp;
}

const std::vector<CaveCarver::StructurePoint>& MarchingCubeHandler::getStructurePoints() const
{
	return m_structurePoints;
}

const std::vector<float3>& MarchingCubeHandler::getPlayerSpawnPositions() const
{
	return m_playerSpawnPositions;
}

void MarchingCubeHandler::drawStructurePoints()
{
	for (auto point : m_structurePoints)
	{
		float4x4 translation = float4x4::CreateTranslation(point.pos + getPosition());
		float4x4 scale = float4x4::CreateScale(point.radius);
		float4x4 mat = scale * translation;

		Graphics::getInstance()->debugDraw_model("sphere.fbx", mat, float3::One);
	}
}

void MarchingCubeHandler::generateData_sphere()
{
	float boundSize = 10;
	float planetRadius = 4;
	float3 id;
	float3 dataSizeMinusOne((float)m_sizeX - 1, (float)m_sizeY - 1, (float)m_sizeZ - 1);

	for (int iz = 0; iz < m_sizeZ; iz++)
	{
		id.z = (float)iz;
		for (int iy = 0; iy < m_sizeY; iy++)
		{
			id.y = (float)iy;
			for (int ix = 0; ix < m_sizeX; ix++)
			{
				id.x = (float)ix;

				// Sphere
				float3 worldPos = ((id / dataSizeMinusOne) - float3(0.5, 0.5, 0.5)) * boundSize;
				float distFromCentre = worldPos.Length();
				float mapValue = distFromCentre - planetRadius;
				setTerrainPixel(ix, iy, iz, (TERRAINDATATYPE)mapValue);

			}
		}
	}
}

void MarchingCubeHandler::generateData_cheese()
{
	for (float iz = 0; iz < m_sizeZ; iz++)
	{
		for (float iy = 0; iy < m_sizeY; iy++)
		{
			for (float ix = 0; ix < m_sizeX; ix++)
			{
				float radius = (m_sizeX + m_sizeZ) / 2.f;
				float angle = 3 / 3.5f;
				float2 pos(ix, iz);
				float2 posNorm(pos);
				posNorm.Normalize();
				float2 one(1, 1);
				one.Normalize();

				if (pos.Length() < radius && posNorm.Dot(one) > angle)
					setTerrainPixel((int)roundf(ix), (int)roundf(iy), (int)roundf(iz), 0);
				else
					setTerrainPixel((int)roundf(ix), (int)roundf(iy), (int)roundf(iz), (TERRAINDATATYPE)m_destroyValue);

				if (iy == 0 || (m_sizeY * 0.6) < iy)
					setTerrainPixel((int)roundf(ix), (int)roundf(iy), (int)roundf(iz), (TERRAINDATATYPE)m_destroyValue);
			}
		}
	}

	//Create cheese holes
	for (size_t i = 0; i < 70; i++)
	{
		float x = RandomFloat(0.0f, getScale().x);
		float y = RandomFloat(0.0f, getScale().y);
		float z = RandomFloat(0.0f, getScale().z);
		float radius = RandomFloat(getScale().x * 0.01f, getScale().x * 0.07f);;
		destroySphere(float3(x, y, z), radius);
	}
}

void MarchingCubeHandler::generateData_fill()
{
	memset(m_terrainData.get(), 0, m_totalSize);

	// comment out in final release. Draws a boundry around the cube

	/*
	for (float iz = 0; iz < m_sizeZ; iz++)
	{
		for (float iy = 0; iy < m_sizeY; iy++)
		{
			for (float ix = 0; ix < m_sizeX; ix++)
			{
				//if (((ix * iy < 20) || (ix == m_sizeX - 2) || (iy == m_sizeY - 2) || (iz == m_sizeZ - 2)) && sin(ix + iy + iz) < 0)
				//setTerrainPixel((int)roundf(ix), (int)roundf(iy), (int)roundf(iz), (TERRAINDATATYPE)m_destroyValue);
				}
		}
	}
	*/

}

void MarchingCubeHandler::generateData_testCave(int nrOfPlayers, unsigned int seed)
{
	Profiler::start("GenerateData_testCave");
	generateData_fill();
	L_System ls;
	CaveCarver cc;
	std::string str;

	float sx = getScale().x;
	float sy = getScale().y;
	float sz = getScale().z;

	// define spawnpoints, in range [0,1] later multiplied with scale (placement temp until we decide something better) 	// |407|
	m_playerSpawnPositions =																									// |2 3|
	{																														// |615|
		{0.5f, 0.8f, 0.8f}, {0.5f, 0.8f, 0.2f}, {0.2f, 0.8f, 0.5f}, {0.8f, 0.8f, 0.5f},
		{0.2f, 0.7f, 0.8f}, {0.8f, 0.7f, 0.2f}, {0.2f, 0.7f, 0.2f}, {0.8f, 0.7f, 0.8f}
	};

	//for (size_t i = 0; i < nrOfPlayers; i++)
	//{
	//	m_playerSpawnPositions.push_back(playerSpawns[i]);
	//}


	// initiate cave L-system
	srand(seed);
	m_latestSeed = seed;
	ls.createBasicRuleSet();

	std::vector<CaveCarver::StructurePoint> newPoints;

	int nrOfCaves[] = { 10,0,0 };	// caveSize 3,4,5

	if (sx > 21.f)
	{
		nrOfCaves[1] += 2;
	}
	if (sx > 41.f)
	{
		nrOfCaves[2] += 2;
	}

	for (int iCaveSize = 0; iCaveSize < 3; iCaveSize++)
	{
		for (size_t i = 0; i < nrOfCaves[iCaveSize]; i++)
		{
			str = ls.runSentence("K", iCaveSize + 3);
			float3 pos = getPosition() + float3(RandomFloat(0, 1), RandomFloat(0, 0.4f), RandomFloat(0, 1)) * getScale();
			cc.createStructurePoints(str, pos, float3(RandomFloat(-1.f, 1.f), RandomFloat(-0.1f, 0.1f), RandomFloat(-1.f, 1.f)));
		}
	}

	cc.ajustPointsToTerrain(*this);
	m_structurePoints = cc.getStructurePoints();	// get structure before carve deletes it
	cc.carveData(*this);							// carve resets structure points here so the spawnpoints later won't get scaled the same


	// create spawn points
	std::vector<CaveCarver::StructurePoint> playerSpawnStructurePoints;
	for (size_t i = 0; i < nrOfPlayers; i++)
	{
		// Define the logic of the spawn point
		float3 pos = m_playerSpawnPositions.at(i) * getScale() + getPosition();
		float3 dir = float3(RandomFloat(-1.f, 1.f), RandomFloat(-0.1f, 0.1f), RandomFloat(-1.f, 1.f));
		str = ls.runSentence("K", 2);

		// Creates a all the structure points based on the provided information
		cc.createStructurePoints(str, pos, dir);
		m_playerSpawnPositions.at(i) = pos;
	}

	// Readjust points so they are within the terrain
	cc.ajustPointsToTerrain(*this);
	m_structurePoints.insert(m_structurePoints.end(), cc.getStructurePoints().begin(), cc.getStructurePoints().end());
	playerSpawnStructurePoints.insert(playerSpawnStructurePoints.end(), cc.getStructurePoints().begin(), cc.getStructurePoints().end());
	cc.carveData(*this);

	// smooth terrain with 4 iterations
	for (size_t i = 0; i < 4; i++)
		smoothTerrain();

	for (size_t i = 0; i < nrOfPlayers; i++)
	{
		CaveCarver::StructurePoint nearestPoint = playerSpawnStructurePoints.at(0);
		float distance = 999999.0f;
		for (size_t y = 0; y < playerSpawnStructurePoints.size(); y++)
		{
			float currentDistance = DirectX::SimpleMath::Vector3::Distance(m_playerSpawnPositions.at(i), playerSpawnStructurePoints.at(y).pos);
			if (currentDistance < distance)
			{
				distance = currentDistance;
				nearestPoint = playerSpawnStructurePoints.at(y);
			}
		}
		m_playerSpawnPositions.at(i) = nearestPoint.pos;
	}
	Profiler::stop();
}

void MarchingCubeHandler::placeDecor()
{
	const float4x4 worldMatrix = getMatrix();

	const int DECOR_COUNT = 5;
	const int MINERAL_COUNT = 1;
	m_decor = {
		DecorCollection("Particle1.fbx"),
		DecorCollection("Particle2.fbx"),
		DecorCollection("Particle3.fbx"),
		DecorCollection("Particle4.fbx"),
		DecorCollection("Particle5.fbx"),

		DecorCollection("Mineral_type0_particle.fbx"),
		//DecorCollection("Mineral_type1_particle.fbx"),
		//DecorCollection("Mineral_type2_particle.fbx"),
		//DecorCollection("Mineral_type3_particle.fbx"),
	};

	size_t decorCounterPlaced = 0;
	for (int iz = 0; iz < m_sizeX; iz++)
	{
		for (int iy = 0; iy < m_sizeY; iy++)
		{
			for (int ix = 0; ix < m_sizeX; ix++)
			{
				int3 node = int3(ix, iy, iz);
				if (isOnEdge(node)) {
					if (RandomFloat(0, 1) > 0.2f)
						continue;

					float3 intersectPos, intersectDir;
					float3 rayPos = float3((float)ix / m_sizeX, (float)iy / m_sizeY, (float)iz / m_sizeZ);
					float3 rayDir = Normalize(float3(RandomFloat(-1, 1), RandomFloat(-1, 1), RandomFloat(-1, 1)));
					float distance = 1.f;

					if (raycast_localSpace(rayPos, rayDir, distance, intersectPos, intersectDir)) {
						if (intersectDir.Dot(float3::Up) < 0.6f)
							continue;

						DecorCollection& collection = m_decor.at(rand() % DECOR_COUNT);
						Transformation transform;
						transform.setPosition(float3::Transform(intersectPos, worldMatrix));
						float3 randDir = float3(RandomFloat(-1, 1), RandomFloat(-1, 1), RandomFloat(-1, 1));
						randDir.Normalize();
						transform.setRotation(DirectX::SimpleMath::Quaternion::CreateFromAxisAngle(randDir, RandomFloat(0, DirectX::XM_2PI)));
						transform.setScale(RandomFloat(1.0f, 4.f));
						float3 color = getTerrainColorFromLocalPosition(intersectPos, intersectDir);
						DecorCollection::DecorInstance instance = { transform.getMatrix(), color };
						collection.m_instances.push_back(instance);
						decorCounterPlaced++;
					}
				}
			}
		}
	}

	// mini mineral scraps
	//{
	//	size_t mineralCounterPlaced = 0;
	//	Scene* scene = getScene();
	//	if (scene != nullptr) {
	//		const std::vector<std::shared_ptr<Mineral>>* minerals = scene->getGameObjects<Mineral>();
	//		if (minerals != nullptr) {
	//			for (size_t iMineral = 0; iMineral < minerals->size(); iMineral++)
	//			{
	//				const Mineral& mineral = *minerals->at(iMineral).get();
	//				float3 position = mineral.getPosition();
	//				float3 normal = mineral.getForward();
	//				float3 color = mineral.getColor();

	//				for (size_t iScraps = 0; iScraps < 30; iScraps++)
	//				{
	//					const float rayPointOffset = 0.5f;
	//					float3 rayPos = position + normal * rayPointOffset;
	//					float3 rayDir = -normal;
	//					float rayDistance = rayPointOffset * 2;
	//					float coneMaxAngle = DegreeToRadian(30);
	//					float coneMinAngle = DegreeToRadian(15);
	//					float rayAngle = RandomFloat(coneMinAngle, coneMaxAngle);
	//					rayDir = CreateVectorFromCone(rayDir, rayAngle, RandomFloat(0, DirectX::XM_2PI));

	//					float3 intersectPos, intersectDir;
	//					if (raycast_worldSpace(rayPos, rayDir, rayDistance, intersectPos, intersectDir)) {

	//						DecorCollection& collection = m_decor.at(RandomInt(DECOR_COUNT, DECOR_COUNT + MINERAL_COUNT - 1)); // random mineral
	//						Transformation transform;
	//						transform.setPosition(intersectPos);
	//						float3 randDir = intersectDir;
	//						float dirAngle = RandomFloat(0, 0);
	//						randDir = CreateVectorFromCone(intersectDir, dirAngle, 0);
	//						transform.setRotation(DirectX::SimpleMath::Quaternion::CreateFromRotationMatrix(float4x4::CreateLookAt(float3(0.f), randDir, float3::Up)));
	//						//transform.rotateByAxis(Normalize(intersectDir.Cross(float3::Up)), DegreeToRadian(90));
	//						//transform.rotateByAxis(intersectDir, RandomFloat(0, DirectX::XM_2PI));
	//						float minScale = 0.2f;
	//						float maxScale = 1.2f;
	//						float scaleInterpolation = Map(rayAngle, coneMinAngle, coneMaxAngle, maxScale, minScale);
	//						transform.setScale(scaleInterpolation);
	//						DecorCollection::DecorInstance instance = { transform.getMatrix(), color };
	//						collection.m_instances.push_back(instance);
	//						mineralCounterPlaced++;
	//					}
	//				}
	//			}
	//		}
	//	}
	//	std::cout << "Minerals Placed: " << mineralCounterPlaced << std::endl;
	//}
}

void MarchingCubeHandler::destroySphere(float3 worldPos, float worldRadius)
{
	float3 pos = translateWorldToDataSpace(worldPos);
	int3 dataStride(m_sizeX / s_nrCubes, m_sizeY / s_nrCubes, m_sizeZ / s_nrCubes);

	float voxelLength = getScale().x / m_sizeX;
	float radius = worldRadius / voxelLength;

	//raise destruction
	for (int iz = (int)max(0, floorf(pos.z - radius)); iz < (int)min(ceilf(pos.z + radius), m_sizeZ - 1); iz++)
	{
		for (int iy = (int)max(0, floorf(pos.y - radius)); iy < (int)min(ceilf(pos.y + radius), m_sizeY - 1); iy++)
		{
			for (int ix = (int)max(0, floorf(pos.x - radius)); ix < (int)min(ceilf(pos.x + radius), m_sizeX - 1); ix++)
			{
				float length = (pos - float3((float)ix, (float)iy, (float)iz)).Length();
				if (length < radius)
				{
					float falloff = (radius - length * 0.9f) / radius;
					setTerrainPixel(ix, iy, iz, (TERRAINDATATYPE)(m_destroyValue * falloff * 0.5f + m_surfaceValue));
				}
				if (length < radius + 2)
				{
					int3 id = int3(ix / dataStride.x, iy / dataStride.y, iz / dataStride.z);
					queueMarchingCube(id);
				}
			}
		}
	}

	// destroy decor
	eraseDecor_sphere(worldPos, worldRadius);
}

void MarchingCubeHandler::damageSphere(float3 worldPos, float worldRadius, float smoothingDataRange)
{
	Profiler::start("damageSphere");
	float3 pos = translateWorldToDataSpace(worldPos);
	int3 dataStride(m_sizeX / s_nrCubes, m_sizeY / s_nrCubes, m_sizeZ / s_nrCubes);

	float voxelLength = getScale().x / m_sizeX;
	float radius = worldRadius / voxelLength;

	//raise destruction
	for (int iz = (int)max(0, floorf(pos.z - radius)); iz < (int)min(ceilf(pos.z + radius), m_sizeZ - 1); iz++)
	{
		for (int iy = (int)max(0, floorf(pos.y - radius)); iy < (int)min(ceilf(pos.y + radius), m_sizeY - 1); iy++)
		{
			for (int ix = (int)max(0, floorf(pos.x - radius)); ix < (int)min(ceilf(pos.x + radius), m_sizeX - 1); ix++)
			{
				float length = (pos - float3((float)ix, (float)iy, (float)iz)).Length();
				if (length < radius)
				{
					float falloff = Map(length, radius - smoothingDataRange, radius, 1, 0);
					float pixelFade = Clamp<float>(Map(falloff, 0, 1, 0, 1), 0, 1);
					float terrainMass = 1.f - (float)getTerrainPixel(ix, iy, iz) / 255.f; // 0 = no terrain, 1 = full terrain
					float newMass = Clamp<float>(terrainMass - pixelFade, 0, 1);
					float newPixelValue = Map(newMass, 0, 1, 255.f, 0.f);
					setTerrainPixel(ix, iy, iz, (TERRAINDATATYPE)(newPixelValue));
				}
				if (length < radius + 2)
				{
					// Find which marching cubes are affected and add them to the update queue if id isn't aleady there.
					// This find will run unnecessarely often. Should look into alternative methods
					int3 id = int3(ix / dataStride.x, iy / dataStride.y, iz / dataStride.z);
					queueMarchingCube(id);
				}
			}
		}
	}
	Profiler::stop();

	// destroy decor
	eraseDecor_sphere(worldPos, worldRadius);
}

void MarchingCubeHandler::damageCylinder(float3 pos, float radius, float height, TERRAINDATATYPE strength)
{
	Profiler::start("damageCylinder");
	pos = translateWorldToDataSpace(pos);
	int3 dataStride(m_sizeX / s_nrCubes, m_sizeY / s_nrCubes, m_sizeZ / s_nrCubes);

	float voxelLength = getScale().x / m_sizeX;
	radius = radius / voxelLength;
	height = height / voxelLength;
	//raise destruction
	for (int iz = (int)max(0, floorf(pos.z - radius)); iz < (int)min(ceilf(pos.z + radius), m_sizeZ - 1); iz++)
	{
		for (int iy = (int)max(0, floorf(pos.y - 1)); iy < (int)min(ceilf(pos.y + height + 1), m_sizeY - 1); iy++)
		{
			for (int ix = (int)max(0, floorf(pos.x - radius)); ix < (int)min(ceilf(pos.x + radius), m_sizeX - 1); ix++)
			{
				float length = (pos - float3((float)ix, pos.y, (float)iz)).Length();
				if (length < radius && (pos.y <= iy && iy < pos.y + height + 1))
				{
					float falloff = (radius - length * 0.4f) / radius;
					float v = getTerrainPixel(ix, iy, iz);

					setTerrainPixel(ix, iy, iz, (TERRAINDATATYPE)min(255, v + strength * falloff));
				}
				if (length < radius + 2)
				{
					// Find which marching cubes are affected and add them to the update queue if id isn't aleady there.
					// This find will run unnecessarely often. Should look into alternative methods
					int3 id = int3(ix / dataStride.x, iy / dataStride.y, iz / dataStride.z);
					queueMarchingCube(id);
				}
			}
		}
	}
	Profiler::stop();
}

void MarchingCubeHandler::smoothTerrain()
{
	static const std::vector<int3> sampleOffsets = {
		int3(1, 0, 0),
		int3(-1, 0, 0),
		int3(0, 1, 0),
		int3(0, -1, 0),
		int3(0, 0, 1),
		int3(0, 0, -1),
	};

	int3 dataStride(m_sizeX / s_nrCubes, m_sizeY / s_nrCubes, m_sizeZ / s_nrCubes);

	for (int iz = 0; iz < m_sizeZ; iz++)
	{
		for (int iy = 0; iy < m_sizeY; iy++)
		{
			for (int ix = 0; ix < m_sizeX; ix++)
			{
				int3 point = int3(ix, iy, iz);
				float3 normal = getDataFieldFlow(point, 1.f);
				if (normal.Length() < 0.0001f)
					continue;
				normal.Normalize();
				float tilt = normal.Dot(float3::Up);
				if (tilt > 0.6f) {
					unsigned char pointFill = getTerrainPixel(ix, iy, iz);

					// collect samples
					std::vector<unsigned char> samples;
					samples.reserve(sampleOffsets.size() + 1);
					for (size_t iSample = 0; iSample < sampleOffsets.size(); iSample++) {
						int3 sampleIdx = point + sampleOffsets[iSample];
						samples.push_back(getTerrainPixel(sampleIdx.x, sampleIdx.y, sampleIdx.z));
					}
					samples.push_back(pointFill);
					// average samples
					int avg = 0;
					for (size_t i = 0; i < samples.size(); i++)
						avg += samples[i];
					avg /= (int)samples.size();
					setTerrainPixel(ix, iy, iz, (unsigned char)avg);

					// enqueue 
					queueMarchingCube_pixelIndex(point);
				}
			}
		}
	}
}

float MarchingCubeHandler::getTerrainValue(float3 worldPos) const
{
	float3 pos = translateWorldToDataSpace(worldPos);
	return getTerrainPixel((int)pos.x, (int)pos.y, (int)pos.z);
}

bool MarchingCubeHandler::isInGround(float3 worldPos)
{
	return getTerrainValue(worldPos) < m_surfaceValue;
}

bool MarchingCubeHandler::isOnEdge(int3 nodeIndex) const
{
	if (getTerrainPixel(nodeIndex.x, nodeIndex.y, nodeIndex.z) > m_surfaceValue)
	{
		for (int ix = -1; ix <= 1; ix++)
		{
			for (int iy = -1; iy <= 1; iy++)
			{
				for (int iz = -1; iz <= 1; iz++)
				{
					if (ix == 0 && iy == 0 && iz == 0)
						continue; // skip middle node
					int3 adjacentIndex = nodeIndex + int3(ix, iy, iz);
					if (adjacentIndex.x < 0 || adjacentIndex.x >= m_sizeX || adjacentIndex.y < 0 || adjacentIndex.y >= m_sizeY || adjacentIndex.z < 0 || adjacentIndex.z >= m_sizeZ)
						continue; // skip adjacent node, it is outside grid
					if (getTerrainPixel(adjacentIndex.x, adjacentIndex.y, adjacentIndex.z) < m_surfaceValue)
						return true;
				}
			}
		}
	}
	return false;
}

bool MarchingCubeHandler::isOnEdge(float3 worldPos) const
{
	float3 localPos = translateWorldToDataSpace(worldPos);
	int3 nodePos(localPos.x, localPos.y, localPos.z);
	return isOnEdge(nodePos);
}

float MarchingCubeHandler::getTriangleMeshSize()
{
	float size = 0;
	for (int iz = 0; iz < s_nrCubes; iz++)
	{
		for (int iy = 0; iy < s_nrCubes; iy++)
		{
			for (int ix = 0; ix < s_nrCubes; ix++)
			{
				size += m_mcs[ix][iy][iz].getTriangleDataSize();
			}
		}
	}
	return size * sizeof(float3) * 2;
}

float MarchingCubeHandler::getTerrainDataSize()
{
	return (float)m_totalSize * sizeof(TERRAINDATATYPE);
}

void MarchingCubeHandler::updateCubesPhysicsActive()
{
	// Gather dynamite positions
	const std::vector<std::shared_ptr<Dynamite>>* dynamites = getScene()->getGameObjects<Dynamite>();
	std::vector<float3> dynamitePositions(dynamites->size());
	for (size_t i = 0; i < dynamites->size(); i++)
	{
		dynamitePositions[i] = dynamites->at(i)->getPosition();
	}

	std::list<int3> newIDs;			// this frame's active IDs
	static float bombScale = 0.6f;

	float borderValues[3] = { 0.5f - (bombScale / (getScale().x / s_nrCubes)), 0.5f - (bombScale / (getScale().y / s_nrCubes)), 0.5f - (bombScale / (getScale().z / s_nrCubes)) };
	//float borderValues[3] = { 0.3 };
	// Find relevant (near bomb) marchingcubes to fill newIDs and activate
	for (size_t i = 0; i < dynamitePositions.size(); i++)
	{
		float3 pos = translateWorldToLocalSpace(dynamitePositions[i]);	// Translate to local space [0,1]
		pos *= s_nrCubes;	// identify which cube the bomb is in	[0, s_nrcubes]
		int3 id = int3((int)pos.x, (int)pos.y, (int)pos.z);
		// if outside terrain cube, skip
		if (id.minimum() < 0 || id.maximum() >= s_nrCubes)
			continue;


		newIDs.push_back(id);

		// TODO: find which cubes the bomb is near and wake them as well
		float position[3] = { pos.x, pos.y, pos.z };	// for loopability
		int3 borderIDs[3] = { int3(0,0,0) };	// offset for border cubes. Used for the corner
		bool addedBorder[3] = { false };
		for (size_t iElement = 0; iElement < 3; iElement++)
		{
			float e = position[iElement];		// [0, s_nrCubes]
			// find which side is closer
			e = fmodf(e, 1.f);			// [0, 1[
			e -= 0.5f;					// [-0.5, 0.5[
			int side = (e < 0.f) ? -1 : 1;
			// if bomb is close enough to a border
			if (abs(e) > borderValues[iElement])
			{
				int3 borderID = int3(side * (iElement == 0 ? 1 : 0), side * (iElement == 1 ? 1 : 0), side * (iElement == 2 ? 1 : 0));
				int3 newId = borderID + id;
				if (newId.minimum() < 0)
					int f = 5;
				if (0 <= newId.minimum() && newId.maximum() < s_nrCubes)
				{
					newIDs.push_back(newId);
					borderIDs[iElement] = borderID;
					addedBorder[iElement] = true;
				}
			}
		}
		int3 newId;
		// if bomb is in a corner
		if (addedBorder[0] && addedBorder[1])
		{
			newId = id + borderIDs[0] + borderIDs[1];
			if (0 <= newId.minimum() && newId.maximum() < s_nrCubes)
				newIDs.push_back(newId);
		}
		if (addedBorder[0] && addedBorder[2])
		{
			newId = id + borderIDs[0] + borderIDs[2];
			if (0 <= newId.minimum() && newId.maximum() < s_nrCubes)
				newIDs.push_back(newId);
		}
		if (addedBorder[1] && addedBorder[2])
		{
			newId = id + borderIDs[1] + borderIDs[2];
			if (0 <= newId.minimum() && newId.maximum() < s_nrCubes)
				newIDs.push_back(newId);
		}

		if (addedBorder[0] && addedBorder[1] && addedBorder[2])
		{
			newId = id + borderIDs[0] + borderIDs[1] + borderIDs[2];
			if (0 <= newId.minimum() && newId.maximum() < s_nrCubes)
				newIDs.push_back(newId);
		}
	}

	// cull bad ids
	//std::list<int3> culledIDs = std::remove_if(newIDs.begin(), newIDs.end(), [](int3 el) {return true; });

	// try to activate simulation on cubes in newIDs
	for (int3 id : newIDs)
	{
		m_mcs[id.x][id.y][id.z].setPhysicsActive(true);
	}

	// remove all elements in newIDs from oldIDs
	for (int3 id : newIDs)
	{
		m_oldIDs.remove(id);
	}

	// deactivate simulation on the remaining marching cubes.
	for (int3 id : m_oldIDs)
	{
		m_mcs[id.x][id.y][id.z].setPhysicsActive(false);
	}

	m_oldIDs = newIDs;
}

float MarchingCubeHandler::getScannerCooldown() const
{
	return m_scannerCooldown / m_scannerMaxCooldown; // normalized cooldown
}

bool MarchingCubeHandler::isScannerActive() const
{
	return m_scanningState != ScannerState::Scan_Inactive;
}

bool MarchingCubeHandler::isWaitingForScannerCooldown() const
{
	return m_waitForCooldown;
}

void MarchingCubeHandler::setScannerPowerupState(bool state)
{
	m_scannerUnlimitedCooldown = state;
	m_scannerCooldown = 0.f;
	m_scannerMaxDistance = (state ? m_scannerPowerupMaxDistance : m_scannerBaseMaxDistance);
}

float3 MarchingCubeHandler::getTerrainColor() const
{
	return float3(m_cbuffer_terrainColor.colorCeil->x, m_cbuffer_terrainColor.colorCeil->y, m_cbuffer_terrainColor.colorCeil->z);
}

void MarchingCubeHandler::relayColorToGraphics()
{
	float4 color;
	float height = getScale().y;
	float cameraHeight = Graphics::getInstance()->getActiveCamera().getPosition().y;
	float portion = cameraHeight / height;
	portion = Clamp(portion * 4, 0.f, 3.99f);

	int index = 3 - (int)portion;
	float rest = 1 - fmodf(portion, 1);

	if (rest < 0.8f || index == 3)
		color = m_terrainColorData.colorCeil[index];
	else
	{
		color = Lerp(m_terrainColorData.colorCeil[index], m_terrainColorData.colorCeil[index + 1], Map(rest, 0.8f, 1.f, 0.f, 1.f));
	}

	Graphics::getInstance()->setFogColor(float3(color.x, color.y, color.z));
}

MarchingCubeHandler::CubeRayCastInfo MarchingCubeHandler::getRayCastInfo() const
{
	return m_rayInfo;
}

float MarchingCubeHandler::measureWallThickness(float3 point1, float3 point2)
{
	float3 rayDir = float3(point2 - point1);
	float distance1 = rayDir.Length(), distance2 = rayDir.Length();
	rayDir.Normalize();
	float3 intersectionPos1, intersectionNormal1, intersectionPos2, intersectionNormal2;

	raycast(point1, rayDir, distance1, intersectionPos1, intersectionNormal1);
	raycast(point2, -rayDir, distance2, intersectionPos2, intersectionNormal2);
	float3 wallThickness = intersectionPos2 - intersectionPos1;

	return wallThickness.Length();
}

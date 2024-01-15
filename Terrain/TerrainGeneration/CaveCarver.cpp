#include "pch.h"
#include "CaveCarver.h"
#include "MarchingCubeHandler.h"

/*	Alphabet table						Macro Table
 ___________________________________	 _________________________________________________________
| F | Move forward		            |	| C | A curve											  |
|---|-------------------------------|	|---|-----------------------------------------------------|
| R | Yaw clockwise		            |	| H | A vertical ascent that returns to horizontal		  |
|---|-------------------------------|	|---|-----------------------------------------------------|
| L | Yaw counterclockwise          |	| Q | A branching structure that generates a room		  |
|---|-------------------------------|	|---|-----------------------------------------------------|
| U | Pitch up			            |	| T | Similar to the H symbol, but splits into two curves |
|---|-------------------------------|	|---|-----------------------------------------------------|
| D | Pitch down	                |	| I | Represents a straightline							  |
|---|-------------------------------|	----------------------------------------------------------
| O | Increase the angle	        |
|---|-------------------------------|
| A | Decrease the angle	        |
|---|-------------------------------|
| B | Step increase		            |
|---|-------------------------------|
| S | Step decrease		            |
|---|-------------------------------|
| Z | The tip of a branch	        |
|---|-------------------------------|
| 0 | Stop connecting other branches|
|---|-------------------------------|
|[ ]| Start/End branch              |
------------------------------------
*/

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

void CaveCarver::createStructurePoints(std::string str, float3 startPos, float3 dir)
{
	if (dir.LengthSquared() == 0)
		dir = float3(1, 0, 0);

	struct Turtle
	{
		Transformation transform;
		float speed = 0.8f;
		float angle = 0.5f;
		float radius = 0.4f;	// how large the ball will be later
	} turtle;

	turtle.transform.setPosition(startPos);
	turtle.transform.lookTo(dir);

	float angleIncrement = 3.14f * 0.1f;
	float speedIncrement = 0.1f;

	std::vector<Turtle> turtleStack;

	for (std::string::iterator iter = str.begin(), end = str.end(); iter != end; iter++)
	{
		// Only atomic instructions are processed
		// If any macros are left, they are ignored
		switch (*iter)
		{
		case 'F':
			turtle.transform.move(turtle.transform.getForward() * turtle.speed);
			m_structurePoints.push_back(StructurePoint(turtle.transform.getPosition(), turtle.speed));
			break;
		case 'R':
			turtle.transform.rotateByAxis(float3::Up, turtle.angle);
			break;
		case 'L':
			turtle.transform.rotateByAxis(float3::Up, -turtle.angle);
			break;
		case 'U':
			turtle.transform.rotateByAxis(float3(1, 0, 0), - turtle.angle);
			break;
		case 'D':
			turtle.transform.rotateByAxis(float3(1, 0, 0), turtle.angle);
			break;
		case 'O':
			turtle.angle += angleIncrement;
			break;
		case 'A':
			turtle.angle -= angleIncrement;
			break;
		case 'B':
			turtle.speed += speedIncrement;
			break;
		case 'S':
			turtle.speed -= speedIncrement;
			break;
		case '[':
			turtleStack.push_back(turtle);
			break;
		case ']':
			turtle = turtleStack.back();
			turtleStack.pop_back();
		}
	}

}

void CaveCarver::ajustPointsToTerrain(MarchingCubeHandler& mc)
{
	float minX = 1000.f, maxX = 0.f, minY = 1000.f, maxY = 0.f, minZ = 1000.f, maxZ = 0.f;
	for (size_t i = 0, size = m_structurePoints.size(); i < size; i++)
	{
		minX = min(minX, m_structurePoints[i].pos.x);
		maxX = max(maxX, m_structurePoints[i].pos.x);
		minY = min(minY, m_structurePoints[i].pos.y);
		maxY = max(maxY, m_structurePoints[i].pos.y);
		minZ = min(minZ, m_structurePoints[i].pos.z);
		maxZ = max(maxZ, m_structurePoints[i].pos.z);
	}

	// scale all structure points' positions to not be close or outside terrain edges 

	float3 minDistanceToWall(1.8f);

	float3 target = minDistanceToWall;
	float3 targetMax = mc.getScale() - minDistanceToWall;
	float3 offsetToZero = -float3(minX, minY, minZ);

	// if out of bounds X | Really wish I knew a nicer way to structure this part
	// afterthought: probably could've been done is defined a [] operator for float3, and defined the minXYZ as float3:s
	if (minX < target.x || maxX > targetMax.x)
	{
		if (maxX - minX > targetMax.x - target.x) // needs rescaling
		{
			for (size_t i = 0, size = m_structurePoints.size(); i < size; i++)
			{
				// move min to origo
				m_structurePoints[i].pos.x += offsetToZero.x;

				// rescale
				m_structurePoints[i].pos.x *= (targetMax.x - target.x) / (maxX - minX);

				// move min to target
				m_structurePoints[i].pos.x += target.x;
			}
		}
		else // just move to target boundry
		{
			float offset;
			if (minX < target.x)
				offset = target.x - minX;
			else
				offset = targetMax.x - maxX;

			// move min to new targetMin
			for (size_t i = 0, size = m_structurePoints.size(); i < size; i++)
			{
				m_structurePoints[i].pos.x += offset;
			}
		}
	}

	// if out of bounds Y
	if (minY < target.y || maxY > targetMax.y)
	{
		if (maxY - minY > targetMax.y - target.y) // needs rescaling
		{
			for (size_t i = 0, size = m_structurePoints.size(); i < size; i++)
			{
				// move min to origo
				m_structurePoints[i].pos.y += offsetToZero.y;

				// rescale
				m_structurePoints[i].pos.y *= (targetMax.y - target.y) / (maxY - minY);

				// move min to target
				m_structurePoints[i].pos.y += target.y;
			}
		}
		else // just move to target boundry
		{
			float offset;
			if (minY < target.y)
				offset = target.y - minY;
			else
				offset = targetMax.y - maxY;

			// move min to new targetMin
			for (size_t i = 0, size = m_structurePoints.size(); i < size; i++)
			{
				m_structurePoints[i].pos.y += offset;
			}
		}
	}

	// if out of bounds Z
	if (minZ < target.z || maxZ > targetMax.z)
	{
		if (maxZ - minZ > targetMax.z - target.z) // needs rescaling
		{
			for (size_t i = 0, size = m_structurePoints.size(); i < size; i++)
			{
				// move min to origo
				m_structurePoints[i].pos.z += offsetToZero.z;

				// rescale
				m_structurePoints[i].pos.z *= (targetMax.z - target.z) / (maxZ - minZ);

				// move min to target
				m_structurePoints[i].pos.z += target.z;
			}
		}
		else // just move to target boundry
		{
			float offset;
			if (minZ < target.z)
				offset = target.z - minZ;
			else
				offset = targetMax.z - maxZ;

			// move min to new targetMin
			for (size_t i = 0, size = m_structurePoints.size(); i < size; i++)
			{
				m_structurePoints[i].pos.z += offset;
			}
		}
	}
}

void CaveCarver::carveData(MarchingCubeHandler& mc)
{
	std::vector<StructurePoint>::iterator iter;
	std::vector<StructurePoint>::iterator end;
	const float damageSmoothDistance = 0.5f;

	// finaly, carve out terrain
	for (size_t i = 0, size = m_structurePoints.size(); i < size; i++)
	{
		mc.damageSphere(m_structurePoints[i].pos + mc.getPosition(), m_structurePoints[i].radius, damageSmoothDistance);
	}

	m_structurePoints.clear();
}

const std::vector<CaveCarver::StructurePoint>& CaveCarver::getStructurePoints() const
{
	return m_structurePoints;
}

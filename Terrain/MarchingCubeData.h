#pragma once
/**
 * baserad från http://paulbourke.net/geometry/polygonise/
 * Original författare: manitoo  Qt/C++ Coding
 *
 */
class MarchingCubeData
{
public:
	/**
	 * Some help functions
	 */

	 // Gets the correct verticepoint along the edge to better represent the surface
	static float getOffset(float value1, float value2, float surfaceValue);

	// Interpolates to a point between the input points based on surfacevlue and the inputpoints "height" value in their w.
	static float3 pointLerp(float4 v1, float4 v2, float surfaceValue);

	// Get a triangle's normal from its vertices.
	static float3 getNormal(float3 v1, float3 v2, float3 v3);

	// The data tables

	// vertexOffset lists the positions, relative to vertex0, of each of the 8 vertices of a cube
	static const float vertexOffset[8][3];

	//edgeConnection lists the index of the endpoint vertices for each of the 12 edges of the cube
	static const int edgeConnection[12][2];

	static const int triangleConnectionTable[256][16];
};

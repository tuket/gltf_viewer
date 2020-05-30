#pragma once

#include <tl/int_types.hpp>

namespace tg
{

void createScreenQuadMesh2D(u32& vao, u32& vbo, u32& numVerts);
void createCubeMesh(u32& vao, u32& vbo, u32& numVerts, bool withNormals);
void createIcoSphereMesh(u32& vao, u32& vbo, u32& numVerts, u32 subDivs, bool withNormals);

// vertex positions of a cube center in the origin of corrdinates with side of length 2
extern const float k_cubeVerts[6*6*3]; // positions only
extern const float k_cubeVertsWithNormals[6*6*(3+3)]; // postions and normals
// vertex positions(2D) for a quad to render in full screen
extern const float k_screenQuad2DVerts[6*2];

}

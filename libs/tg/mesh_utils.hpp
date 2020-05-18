#pragma once

#include <tl/int_types.hpp>

namespace tg
{

void createScreenQuadMesh2D(u32& vao, u32& vbo, u32& numVerts);

// vertex positions of a cube center in the origin of corrdinates with side of length 2
extern const float cubeVerts[6*6*(3+2)];
// vertex positions(2D) for a quad to render in full screen
extern const float screenQuad2DVerts[6*2];

}

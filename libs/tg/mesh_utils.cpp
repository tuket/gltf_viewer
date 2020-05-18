#include "mesh_utils.hpp"

#include <glad/glad.h>

namespace tg
{

void createScreenQuadMesh2D(u32& vao, u32& vbo, u32& numVerts)
{
    numVerts = 6;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(screenQuad2DVerts), screenQuad2DVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
}

const float cubeVerts[6*6*(3+2)] = {
    // x, y, z,
    // LEFT
    -1, -1, -1,
    -1, -1, +1,
    -1, +1, +1,
    -1, -1, -1,
    -1, +1, +1,
    -1, +1, -1,
    // RIGHT
    +1, -1, +1,
    +1, -1, -1,
    +1, +1, -1,
    +1, -1, +1,
    +1, +1, -1,
    +1, +1, +1,
    // BOTTOM
    -1, -1, -1,
    +1, -1, -1,
    +1, -1, +1,
    -1, -1, -1,
    +1, -1, +1,
    -1, -1, +1,
    // TOP
    -1, +1, +1,
    +1, +1, +1,
    +1, +1, -1,
    -1, +1, +1,
    +1, +1, -1,
    -1, +1, -1,
    // FRONT
    -1, -1, +1,
    +1, -1, +1,
    +1, +1, +1,
    -1, -1, +1,
    +1, +1, +1,
    -1, +1, +1,
    // BACK
    +1, -1, -1,
    -1, -1, -1,
    -1, +1, -1,
    +1, -1, -1,
    -1, +1, -1,
    +1, +1, -1,
};

extern const float screenQuad2DVerts[6*2] = {
    -1, -1,  +1, -1,  +1, +1
    -1, -1,  +1, +1,  -1, +1
};

}

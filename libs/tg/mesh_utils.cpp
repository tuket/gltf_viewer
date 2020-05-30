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
    glBufferData(GL_ARRAY_BUFFER, sizeof(k_screenQuad2DVerts), k_screenQuad2DVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
}

void createCubeMesh(u32& vao, u32& vbo, u32& numVerts, bool withNormals)
{
    numVerts = 36;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    if(withNormals)
        glBufferData(GL_ARRAY_BUFFER, sizeof(k_cubeVertsWithNormals), k_cubeVertsWithNormals, GL_STATIC_DRAW);
    else
        glBufferData(GL_ARRAY_BUFFER, sizeof(k_cubeVerts), k_cubeVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), nullptr);
    if(withNormals) {
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    }
}

void createIcoSphereMesh(u32& vao, u32& vbo, u32& numVerts, u32 subDivs, bool withNormals)
{

}

const float k_cubeVerts[6*6*3] = {
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

const float k_cubeVertsWithNormals[6*6*(3+3)] =
{
        // x, y, z,
        // LEFT
        -1, -1, -1,  -1, 0, 0,
        -1, -1, +1,  -1, 0, 0,
        -1, +1, +1,  -1, 0, 0,
        -1, -1, -1,  -1, 0, 0,
        -1, +1, +1,  -1, 0, 0,
        -1, +1, -1,  -1, 0, 0,
        // RIGHT
        +1, -1, +1,  +1, 0, 0,
        +1, -1, -1,  +1, 0, 0,
        +1, +1, -1,  +1, 0, 0,
        +1, -1, +1,  +1, 0, 0,
        +1, +1, -1,  +1, 0, 0,
        +1, +1, +1,  +1, 0, 0,
        // BOTTOM
        -1, -1, -1,  0, -1, 0,
        +1, -1, -1,  0, -1, 0,
        +1, -1, +1,  0, -1, 0,
        -1, -1, -1,  0, -1, 0,
        +1, -1, +1,  0, -1, 0,
        -1, -1, +1,  0, -1, 0,
        // TOP
        -1, +1, +1,  0, +1, 0,
        +1, +1, +1,  0, +1, 0,
        +1, +1, -1,  0, +1, 0,
        -1, +1, +1,  0, +1, 0,
        +1, +1, -1,  0, +1, 0,
        -1, +1, -1,  0, +1, 0,
        // FRONT
        -1, -1, +1,  0, 0, +1,
        +1, -1, +1,  0, 0, +1,
        +1, +1, +1,  0, 0, +1,
        -1, -1, +1,  0, 0, +1,
        +1, +1, +1,  0, 0, +1,
        -1, +1, +1,  0, 0, +1,
        // BACK
        +1, -1, -1,  0, 0, -1,
        -1, -1, -1,  0, 0, -1,
        -1, +1, -1,  0, 0, -1,
        +1, -1, -1,  0, 0, -1,
        -1, +1, -1,  0, 0, -1,
        +1, +1, -1,  0, 0, -1,
};

const float k_screenQuad2DVerts[6*2] = {
    -1, -1,  +1, -1,  +1, +1,
    -1, -1,  +1, +1,  -1, +1
};

}

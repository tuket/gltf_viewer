#include "shader_utils.hpp"

#include <stdio.h>
#include <assert.h>
#include <glad/glad.h>
#include <tl/defer.hpp>
#include "internal.hpp"

namespace tg
{

char* getShaderCompileErrors(u32 shader, tl::Span<char> buffer)
{
    i32 ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if(!ok) {
        GLsizei outSize;
        glGetShaderInfoLog(shader, buffer.size(), &outSize, buffer.begin());
        return buffer.begin();
    }
    return nullptr;
}

char* getShaderLinkErrors(u32 prog, tl::Span<char> buffer)
{
    GLint success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(prog, buffer.size(), nullptr, buffer.begin());
        return buffer.begin();
    }
    return nullptr;
}

static const char s_simpleCubemapVertShadSrc[] =
R"GLSL(
#version 330 core

layout(location = 0) in vec3 a_pos;

out vec3 v_modelPos;

uniform mat4 u_modelViewProjMat;

void main()
{
    v_modelPos = a_pos;
    gl_Position = u_modelViewProjMat * vec4(a_pos, 1);
}
)GLSL";

static const char s_simpleCubemapFragShadSrc[] =
R"GLSL(
#version 330 core

layout(location = 0) out vec4 o_color;

in vec3 v_modelPos;

uniform samplerCube u_cubemap;
uniform float u_gammaExponent = 1.0;

void main()
{
    o_color = texture(u_cubemap, normalize(v_modelPos));
    o_color.rbg = pow(o_color.rbg, vec3(u_gammaExponent));
}
)GLSL";

void createSimpleCubemapShader(u32& prog,
    i32& modelViewProjUnifLoc, i32& cubemapTexUnifLoc, i32& gammaExpUnifLoc)
{
    prog = glCreateProgram();

    u32 vertShad = glCreateShader(GL_VERTEX_SHADER);
    defer(glDeleteShader(vertShad));
    const char* vertShadSrc = s_simpleCubemapVertShadSrc;
    glShaderSource(vertShad, 1, &vertShadSrc, nullptr);
    glCompileShader(vertShad);
    if(const char* errorMsg = tg::getShaderCompileErrors(vertShad, g_buffer)) {
        printf("Error compiling vertex shader:\n%s\n", errorMsg);
        assert(false);
    }

    u32 fragShad = glCreateShader(GL_FRAGMENT_SHADER);
    defer(glDeleteShader(fragShad));
    const char* fragShadSrc = s_simpleCubemapFragShadSrc;
    glShaderSource(fragShad, 1, &fragShadSrc, nullptr);
    glCompileShader(fragShad);
    if(const char* errorMsg = tg::getShaderCompileErrors(fragShad, g_buffer)) {
        printf("Error compiling fragment shader:\n%s\n", errorMsg);
        assert(false);
    }

    glAttachShader(prog, vertShad);
    glAttachShader(prog, fragShad);
    glLinkProgram(prog);
    if(const char* errorMsg = tg::getShaderLinkErrors(prog, g_buffer)) {
        printf("Error linking:\n%s\n", errorMsg);
        assert(false);
    }

    modelViewProjUnifLoc = glGetUniformLocation(prog, "u_modelViewProjMat");
    cubemapTexUnifLoc = glGetUniformLocation(prog, "u_cubemap");
    gammaExpUnifLoc = glGetUniformLocation(prog, "u_gammaExponent");
}

namespace srcs
{

const char* header =
R"GLSL(
#version 330 core

const float PI = 3.14159265359;
)GLSL";

const char* hammersley =
R"GLSL(
vec2 hammersleyVec2(uint i, uint numSamples)
{
    uint b = (i << 16u) | (i >> 16u);
    b = ((b & 0x55555555u) << 1u) | ((b & 0xAAAAAAAAu) >> 1u);
    b = ((b & 0x33333333u) << 2u) | ((b & 0xCCCCCCCCu) >> 2u);
    b = ((b & 0x0F0F0F0Fu) << 4u) | ((b & 0xF0F0F0F0u) >> 4u);
    b = ((b & 0x00FF00FFu) << 8u) | ((b & 0xFF00FF00u) >> 8u);
    float radicalInverseVDC = float(b) * 2.3283064365386963e-10;
    return vec2(float(i) / float(numSamples), radicalInverseVDC);
}
)GLSL";

const char* pcg_uvec3_uvec3 =
R"GLSL(
uvec3 pcg_uvec3_uvec3(uvec3 v)
{
    v = v * 1664525u + 1013904223u;
    v.x += v.y*v.z;
    v.y += v.z*v.x;
    v.z += v.x*v.y;
    v = v ^ (v>>16u);
    v.x += v.y*v.z;
    v.y += v.z*v.x;
    v.z += v.x*v.y;
    return v;
}
)GLSL";

const char* importanceSampleGgxD =
R"GLSL(
vec3 importanceSampleGgxD(vec2 seed, float rough2, vec3 N)
{
    float phi = 2.0 * PI * seed.x;
    float cosTheta = sqrt((1.0 - seed.y) / (1 + (rough2*rough2 - 1) * seed.y));
    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
    /*vec3 h;
    h.x = sinTheta * cos(phi);
    h.y = sinTheta * sin(phi);
    h.z = cosTheta;
    vec3 up = abs(N.y) < 0.99 ? vec3(0, 1, 0) : vec3(1, 0, 0);
    vec3 tangentX = normalize(cross(up, N));
    vec3 tangentZ = cross(tangentX, N);
    return h.x * tangentX + h.y * up + h.z * tangentZ;*/

    vec3 H;
    H.x = sinTheta * cos( phi );
    H.y = sinTheta * sin( phi );
    H.z = cosTheta;
    vec3 UpVector = abs(N.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
    vec3 TangentX = normalize( cross( UpVector , N ) );
    vec3 TangentY = cross( N, TangentX );
    // Tangent to world space
    return TangentX * H.x + TangentY * H.y + N * H.z;
}
)GLSL";

const char* uniformSample =
R"GLSL(
vec3 uniformSample(vec2 seed, vec3 N)
{
    float phi = 2 * PI * seed.x;
    float r = sqrt(1 - seed.y*seed.y);
    vec3 v = vec3(r*cos(phi), seed.y, r*sin(phi));
    v = normalize(v);
    vec3 up = vec3(0, 1, 0);
    if(dot(up, N) > 0.99)
        up = vec3(1, 0, 0);
    vec3 X = normalize(cross(N, up));
    vec3 Z = cross(X, N);
    return X * v.x + N * v.y + Z * v.z;
}
)GLSL";

}

}

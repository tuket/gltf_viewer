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

in layout(location = 0) vec3 a_pos;

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

}

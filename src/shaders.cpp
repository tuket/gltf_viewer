#include "shaders.hpp"

#include <tl/str.hpp>
#include <tl/fmt.hpp>
#include <glad/glad.h>

namespace gpu
{

namespace src
{
static const char version[] = "#version 330 core\n\n";

static const char basicVertShader[] =
R"GLSL(
uniform mat3 u_modelView3;
uniform mat4 u_modelViewProj;

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec3 a_tangent;
layout(location = 3) in vec3 a_bitangent;
layout(location = 4) in vec2 a_texCoord0;
layout(location = 5) in vec2 a_texCoord1;
layout(location = 6) in vec4 a_color;

out vec3 v_normal;
out vec2 v_texCoord0;
out vec2 v_texCoord1;
out vec4 v_color;

void main()
{
    gl_Position = u_modelViewProj * vec4(a_pos, 1.0);
    mat3 TBN = mat3(a_tangent, a_bitangent, a_normal);
    v_normal = u_modelView3 * TBN * a_normal;
    v_texCoord0 = a_texCoord0;
    v_texCoord1 = a_texCoord1;
    v_color = a_color;
}

)GLSL";

static const char pbrMetallic[] =
R"GLSL(
layout(location = 0) out vec4 o_color;

uniform sampler2D u_albedoTexture;
uniform sampler2D u_metallicRoughnessTexture;
uniform vec3 u_lightDir;
uniform vec3 u_lightColor;

in vec3 v_normal;
in vec2 v_texCoord0;
in vec2 v_texCoord1;
in vec4 v_color;

void main()
{
    o_color = vec4(1.0, 0.0, 0.0, 1.0);
}

)GLSL";

} // namesapce src

template <i32 N>
static inline i32 strlenGeneric(const char (&)[N]) { return N-1; }
static inline i32 strlenGeneric(const char* str) { return (i32)tl::strlen(str); }
static inline i32 strlenGeneric(CStr str) { return (i32)str.size(); }

template <typename... Ts>
void uploadShaderSources(u32 shader, const Ts&... xs)
{
    const char* const srcs[] = { xs... };
    const i32 sizes[] = { strlenGeneric(xs)... };
   glShaderSource(shader, sizeof...(xs), srcs, sizes);
}

namespace progs
{
    static u32 pbrMetallic = 0;
}

constexpr u32 infoLogSize = 32*1024;
static char infoLog[infoLogSize];

static const char* shaderCompileErrs(u32 shader)
{
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(shader, infoLogSize, nullptr, infoLog);
        return infoLog;
    }
    return nullptr;
}

static const char* programLinkErrs(u32 prog)
{
    GLint success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if(!success) {
        glGetProgramInfoLog(prog, infoLogSize, nullptr, infoLog);
    }
    return nullptr;
}

bool buildShaders()
{
    const u32 vertShader = glCreateShader(GL_VERTEX_SHADER);
    uploadShaderSources(vertShader, src::version, src::basicVertShader);
    glCompileShader(vertShader);
    if(const char* errs = shaderCompileErrs(vertShader)) {
        tl::printError(errs);
        return false;
    }

    const u32 metallicShader = glCreateShader(GL_FRAGMENT_SHADER);
    uploadShaderSources(metallicShader, src::version, src::pbrMetallic);
    glCompileShader(metallicShader);
    if(const char* errs = shaderCompileErrs(metallicShader)) {
        tl::printError(errs);
        return false;
    }

    progs::pbrMetallic = glCreateProgram();
    glAttachShader(progs::pbrMetallic, vertShader);
    glAttachShader(progs::pbrMetallic, metallicShader);
    glLinkProgram(progs::pbrMetallic);
    if(const char* errs = programLinkErrs(progs::pbrMetallic)) {
        tl::printError(errs);
        return false;
    }

    return true;
}

u32 shaderPbrMetallic()
{
    return progs::pbrMetallic;
}

u32 shaderPbrGloss()
{
    assert(false && "not implemented");
    return 0;
}

}

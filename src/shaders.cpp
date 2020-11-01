#include "shaders.hpp"

#include <tl/str.hpp>
#include <tl/fmt.hpp>
#include <glad/glad.h>
#include "utils.hpp"
#include <tg/shader_utils.hpp>

namespace gpu
{

namespace src
{
static const char version[] = "#version 330 core\n\n";

static const char basicVertShader[] =
R"GLSL(
uniform mat3 u_modelMat3;
uniform mat4 u_modelViewProj;
uniform vec4 u_color;

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec3 a_tangent;
layout(location = 3) in vec3 a_bitangent;
layout(location = 4) in vec2 a_texCoord0;
layout(location = 5) in vec2 a_texCoord1;
layout(location = 6) in vec4 a_color;

out vec3 v_normal;
out vec3 v_tangent;
out vec3 v_bitangent;
out vec2 v_texCoord0;
out vec2 v_texCoord1;
out vec4 v_color;

void main()
{
    gl_Position = u_modelViewProj * vec4(a_pos, 1.0);
    v_normal = u_modelMat3 * a_normal;
    v_tangent = u_modelMat3 * a_tangent;
    v_bitangent = u_modelMat3 * a_bitangent;
    v_texCoord0 = a_texCoord0;
    v_texCoord1 = a_texCoord1;
    v_color = u_color * a_color;
}

)GLSL";

static const char pbrMetallic[] =
R"GLSL(
layout(location = 0) out vec4 o_color;

uniform sampler2D u_colorTexture;
uniform sampler2D u_normalTexture;
uniform sampler2D u_metallicRoughnessTexture;
uniform vec3 u_lightDir;
uniform vec3 u_lightColor;

in vec3 v_normal;
in vec3 v_tangent;
in vec3 v_bitangent;
in vec2 v_texCoord0;
in vec2 v_texCoord1;
in vec4 v_color;

void main()
{
    mat3 TBN = mat3(v_tangent, v_bitangent, v_normal);
    vec3 normal = TBN * texture(u_normalTexture, v_texCoord0).xyz;
    vec4 texColor = texture(u_colorTexture, v_texCoord0);
    o_color = vec4(
        dot(normal, normalize(vec3(0.2, 1.0, 0.5))) * mix(texColor.rgb * v_color.rgb, vec3(v_texCoord0, 0.0), 0.01),
        texColor.a * v_color.a
    );
    //o_color = vec4(v_texCoord0, 0.0, 1.0);
    //o_color = vec4(abs(v_normal), 1.0);
}

)GLSL";

static const char vertColor_vert[] =
R"GLSL(
uniform mat4 u_modelViewProj;

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_color;

out vec3 v_color;

void main()
{
    v_color = a_color;
    gl_Position = u_modelViewProj * vec4(a_pos, 1);
}
)GLSL";

static const char vertColor_frag[] =
R"GLSL(
layout(location = 0) out vec4 o_color;

in vec3 v_color;

void main()
{
    o_color = vec4(v_color, 1);
}
)GLSL";

static const char onlyPos_vert[] =
R"GLSL(
uniform mat4 u_modelView;
uniform mat4 u_modelViewProj;

layout (location = 0) in vec3 a_pos;
out vec3 v_camSpacePos;

void main()
{
    v_camSpacePos = vec3(u_modelView * vec4(a_pos, 1));
    gl_Position = u_modelViewProj * vec4(a_pos, 1);
}
)GLSL";

static const char floorGrid_frag[] =
R"GLSL(
layout (location = 0) out vec4 o_color;
in vec3 v_camSpacePos;
uniform vec4 u_color;
uniform float u_distToFloor;

void main()
{
    float depth = length(v_camSpacePos);
    float distAlpha = pow(3*u_distToFloor / depth, 1.7);
    distAlpha = min(distAlpha, 1);
    o_color = vec4(u_color.rgb, u_color.a * distAlpha);
}
)GLSL";

} // namespace src

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

namespace sd
{
    static ShaderData pbrMetallic;
    static ShaderData_VertColor vertColor;
    static ShaderData_FloorGrid floorGrid;
}

constexpr u32 infoLogSize = 32*1024;
static char infoLog[infoLogSize];

void findAllUnifLocations(ShaderData& data)
{
   data.unifLocs.modelMat3 = glGetUniformLocation(data.prog, "u_modelMat3");
   data.unifLocs.modelViewProj = glGetUniformLocation(data.prog, "u_modelViewProj");
   data.unifLocs.color = glGetUniformLocation(data.prog, "u_color");
//   data.unifLocs.colorTexture = glGetUniformLocation(data.prog, "u_colorTex");
//   data.unifLocs.normalTexture = glGetUniformLocation(data.prog, "u_normalTex");
}

bool buildShaders()
{
    const u32 vertShader = glCreateShader(GL_VERTEX_SHADER);
    uploadShaderSources(vertShader, src::version, src::basicVertShader);
    glCompileShader(vertShader);
    if(const char* errs = tg::getShaderCompileErrors(vertShader, infoLog)) {
        tl::printError(errs);
        return false;
    }

    { // metallic
        const u32 fragShader = glCreateShader(GL_FRAGMENT_SHADER);
        uploadShaderSources(fragShader, src::version, src::pbrMetallic);
        glCompileShader(fragShader);
        if(const char* errs = tg::getShaderCompileErrors(fragShader, infoLog)) {
            tl::printError(errs);
            return false;
        }

        auto& data = sd::pbrMetallic;
        data.prog = glCreateProgram();
        glAttachShader(data.prog, vertShader);
        glAttachShader(data.prog, fragShader);
        glLinkProgram(data.prog);
        if(const char* errs = tg::getShaderLinkErrors(data.prog, infoLog)) {
            tl::printError(errs);
            glDeleteShader(fragShader);
            glDeleteProgram(data.prog);
            return false;
        }
        findAllUnifLocations(data);
        glDeleteShader(fragShader);
        glUseProgram(data.prog);
        glUniform1i(glGetUniformLocation(data.prog, "u_colorTexture"), (i32)ETexUnit::ALBEDO);
        glUniform1i(glGetUniformLocation(data.prog, "u_normalTexture"), (i32)ETexUnit::NORMAL);
        glUniform1i(glGetUniformLocation(data.prog, "u_metallicRoughnessTexture"), (i32)ETexUnit::PHYSICS);
    }

    { // shader vert color
        const u32 vertShad = glCreateShader(GL_VERTEX_SHADER);
        uploadShaderSources(vertShad, src::version, src::vertColor_vert);
        glCompileShader(vertShad);
        if(const char* errs = tg::getShaderCompileErrors(vertShad, infoLog)) {
            tl::printError(errs);
            return false;
        }

        const u32 fragShad = glCreateShader(GL_FRAGMENT_SHADER);
        uploadShaderSources(fragShad, src::version, src::vertColor_frag);
        glCompileShader(fragShad);
        if(const char* errs = tg::getShaderCompileErrors(fragShad, infoLog)) {
            tl::printError(errs);
            return false;
        }

        auto& data = sd::vertColor;
        data.prog = glCreateProgram();
        glAttachShader(data.prog, vertShad);
        glAttachShader(data.prog, fragShad);
        glLinkProgram(data.prog);
        if(const char* errs = tg::getShaderLinkErrors(data.prog, infoLog)) {
            tl::printError(errs);
            glDeleteShader(vertShad);
            glDeleteShader(fragShad);
            glDeleteProgram(data.prog);
            return false;
        }
        glDetachShader(data.prog, vertShad);
        glDetachShader(data.prog, fragShad);
        glDeleteShader(vertShad);
        glDeleteShader(fragShad);

        data.locs.modelViewProj = glGetUniformLocation(data.prog, "u_modelViewProj");
    }

    { // shader floor grid
        const u32 vertShad = glCreateShader(GL_VERTEX_SHADER);
        uploadShaderSources(vertShad, src::version, src::onlyPos_vert);
        glCompileShader(vertShad);
        if(const char* errs = tg::getShaderCompileErrors(vertShad, infoLog)) {
            tl::printError(errs);
            return false;
        }

        const u32 fragShad = glCreateShader(GL_FRAGMENT_SHADER);
        uploadShaderSources(fragShad, src::version, src::floorGrid_frag);
        glCompileShader(fragShad);
        if(const char* errs = tg::getShaderCompileErrors(fragShad, infoLog)) {
            tl::printError(errs);
            return false;
        }

        auto& data = sd::floorGrid;
        data.prog = glCreateProgram();
        glAttachShader(data.prog, vertShad);
        glAttachShader(data.prog, fragShad);
        glLinkProgram(data.prog);
        if(const char* errs = tg::getShaderLinkErrors(data.prog, infoLog)) {
            tl::printError(errs);
            glDeleteShader(vertShad);
            glDeleteShader(fragShad);
            glDeleteProgram(data.prog);
            return false;
        }
        glDetachShader(data.prog, vertShad);
        glDetachShader(data.prog, fragShad);
        glDeleteShader(vertShad);
        glDeleteShader(fragShad);

        data.locs.modelView = glGetUniformLocation(data.prog, "u_modelView");
        data.locs.modelViewProj = glGetUniformLocation(data.prog, "u_modelViewProj");
        data.locs.color = glGetUniformLocation(data.prog, "u_color");
        data.locs.distToFloor = glGetUniformLocation(data.prog, "u_distToFloor");
    }

    glDeleteShader(vertShader);
    return true;
}

const ShaderData& shaderPbrMetallic()
{
    return sd::pbrMetallic;
}

const ShaderData& shaderPbrGloss()
{
    assert(false && "not implemented");
    static const ShaderData temp{};
    return temp;
}

const ShaderData_VertColor& shaderVertColor()
{
    return sd::vertColor;
}

const ShaderData_FloorGrid& shaderFloorGrid()
{
    return sd::floorGrid;
}

}

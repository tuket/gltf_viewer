#pragma once

#include <tl/int_types.hpp>

struct UniformLocations {
    i32 modelViewProj,
        modelMat3,
        color,
        colorTexture,
        normalTexture;
};

struct ShaderData {
    u32 prog;
    UniformLocations unifLocs;
};

struct ShaderData_VertColor {
    u32 prog;
    struct {
        u32 modelViewProj;
    } locs;
};

namespace gpu
{

bool buildShaders();

const ShaderData& shaderPbrMetallic();
const ShaderData& shaderPbrGloss();
const ShaderData_VertColor shaderVertColor();

}

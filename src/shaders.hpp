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

namespace gpu
{

bool buildShaders();

const ShaderData& shaderPbrMetallic();
const ShaderData& shaderPbrGloss();

}

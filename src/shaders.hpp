#pragma once

#include <tl/int_types.hpp>

namespace gpu
{

bool buildShaders();

u32 shaderPbrMetallic();
u32 shaderPbrGloss();

}

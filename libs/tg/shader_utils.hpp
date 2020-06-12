#pragma once

#include <tl/int_types.hpp>
#include <tl/span.hpp>

namespace tg
{

char* getShaderCompileErrors(u32 shad, tl::Span<char> buffer);
char* getShaderLinkErrors(u32 prog, tl::Span<char> buffer);

void createSimpleCubemapShader(u32& prog,
    i32& modelViewProjUnifLoc, i32& cubemapTexUnifLoc, i32& gammaExpUnifLoc);

namespace srcs
{
extern const char* header;
extern const char* hammersley;
extern const char* importanceSampleGgxD;
}

}

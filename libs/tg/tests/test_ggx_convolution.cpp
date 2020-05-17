#include <tl/int_types.hpp>
#include <tl/fmt.hpp>
#include <tg/texture_utils.hpp>
#include <tg/geometry_utils.hpp>
#include <tg/shader_utils.hpp>
#include <glad/glad.h>
#include "test_utils.hpp"

static char s_buffer[4*1024];

bool test_ggxConvolution()
{
    auto window = simpleInitGlfwGL();

    const u32 vertShader = tg::createFilterCubemapVertShader();
    const u32 fragShader = tg::createFilterCubemap_ggx_fragShader();
    const u32 prog = glCreateProgram();
    glAttachShader(prog, vertShader);
    glAttachShader(prog, fragShader);
    glLinkProgram(prog);
    if(const char* errMsg = tg::getShaderLinkErrors(prog, s_buffer)) {
        tl::println("Error linking shader:\n", errMsg);
        return false;
    }
    const tg::GgxFilterUnifLocs unifLocs = tg::getFilterCubamap_ggx_unifLocs(prog);
    u32 vao, vbo, numVerts;
    tg::createFilterCubemapMeshGpu(vao, vbo, numVerts);
    auto error = tg::filterCubemap_GGX(
        "test.hdr",
        "autumn_ggx_", ".hdr",
        prog, vao, unifLocs);
    if(error == tg::FilterCubemapError::CANT_OPEN_INPUT_FILE) {
        tl::println("Error: could not open input file");
    }
    else if(error == tg::FilterCubemapError::CANT_OPEN_OUTPUT_FILE){
        tl::println("Error: could not write to output files");
    }
    return error != tg::FilterCubemapError::NONE;
}

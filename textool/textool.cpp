#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stbi.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <tl/span.hpp>
#include <tl/containers/fvector.hpp>
#include <tl/basic.hpp>
#include <tl/basic_math.hpp>
#include <tg/texture_utils.hpp>
#include <tg/shader_utils.hpp>
#include <tl/str.hpp>

using glm::vec2;
using glm::vec3;

static const char s_buffer[4*1024];

bool filterCubemap(int argc, char* argv[])
{
    auto printUsage = []() {
        printf("usage:\n"
            "textool filter_cubemap <brdf> <input_texture> <output_texture_preffix> <output_texture_extension>\n"
            "<brdf> can be: {ggx}\n"
        );
    };
    if(argc != 6) {
        printUsage();
        return false;
    }
    CStr brdf = argv[2];
    CStr inTex = argv[3];
    CStr outTexPreffix = argv[4];
    CStr outTexExtension = argv[5];
    if(brdf == "ggx")
    {
        const u32 vertShad = tg::createFilterCubemapVertShader();
        const u32 fragShad = tg::createFilterCubemap_ggx_fragShader();
        const u32 prog = glCreateProgram();
        glAttachShader(prog, vertShad);
        glAttachShader(prog, fragShad);
        glLinkProgram(prog);
        const char* errorMsg = tg::getShaderLinkErrors(s_buffer);
        assert(!errorMsg);
        tg::GgxFilterUnifLocs locs = tg::getFilterCubamap_ggx_unifLocs(prog);
        u32 vao, vbo, numVerts;
        tg::createCubemapMeshGpu(vao, vbo, numVerts);

        glUseProgram(prog);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, numVerts);

        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
        glDeleteShader(vertShad);
        glDeleteShader(fragShad);
        glDeleteProgram(prog);
        return true;
    }
    else {
        printUsage();
        return false;
    }
    return true;
}

int main(int argc, char* argv[])
{
    if(argc != 17) {
        printf("incorrect number of args\n"
               "usage: filter_cubemap inputTexture outputTexture "
               "faceW faceH "
               "leftFaceX leftFaceY rightFaceX rightFaceY bottomFaceX bottomFaceY topFaceX topFaceY frontFaceX frontFaceY backFaceX backFaceY\n");
        return 1;
    }
    const int faceW = atoi(argv[3]);
    const int faceH = atoi(argv[4]);
    const vec2 facesCoords[6] = {
        {atoi(argv[5]), atoi(argv[6])},
        {atoi(argv[7]), atoi(argv[8])},
        {atoi(argv[9]), atoi(argv[10])},
        {atoi(argv[11]), atoi(argv[12])},
        {atoi(argv[13]), atoi(argv[14])},
        {atoi(argv[15]), atoi(argv[16])}
    };
    const FilterCubemapError error = filterCubemap(argv[1], argv[2], faceW, faceH, facesCoords);
    switch(error) {
    case FilterCubemapError::CANT_OPEN_INPUT_FILE:
        printf("Error: can't open input file\n");
        break;
    case FilterCubemapError::CANT_OPEN_OUTPUT_FILE:
        printf("Error: can't open output file\n");
        break;
    }
}

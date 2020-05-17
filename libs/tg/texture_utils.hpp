#pragma once

#include <tl/containers/fvector.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include "img.hpp"

namespace tg
{

enum class FilterCubemapError {
    NONE,
    CANT_OPEN_INPUT_FILE,
    CANT_OPEN_OUTPUT_FILE,
};

/* The layout of the input cubemap image file is assumed to be:

         +---------+
         |         |
         |   up    |
         |         |
+---------------------------+--------+
|        |         |        |        |
|  left  |  front  |  right |  back  |
|        |         |        |        |
+---------------------------+--------+
         |         |
         |  down   |
         |         |
         +---------+
*/


u32 createFilterCubemapVertShader();
void createFilterCubemapMeshGpu(u32& vao, u32& vbo, u32& numVerts);

struct GgxFilterUnifLocs {
    i32 cubemap, numSamples, roughness2;
};
u32 createFilterCubemap_ggx_fragShader();
GgxFilterUnifLocs getFilterCubamap_ggx_unifLocs(u32 prog);


void filterCubemap_GGX(tl::FVector<Img3f, 16>& outMips,
    ImgView3f inImg,
    u32 shaderProg, // create with createFilterCubemapVertShader()
    u32 cubemapMeshVao, // create with createFilterCubemapMeshGpu();
    const GgxFilterUnifLocs& locs);

FilterCubemapError filterCubemap_GGX(const char* inImgFileName,
    const char* outImgFileNamePrefix, const char* outImgExtension,
    u32 shaderProg, // create with createFilterCubemapVertShader()
    u32 filterCubemapMeshVao, // create with createFilterCubemapMeshGpu();
    const GgxFilterUnifLocs& locs);

void cylinderMapToCubeMap(CubeImgView3f cube, CImg3f cylindricMap);

void uploadCubemapTexture(u32 mipLevel, u32 w, u32 h, u32 internalFormat, u32 dataFormat, u32 dataType, u8* data);

CImg3f gerateGgxLutImg(u32 sizePixels, u32 screenQuadVao);

}

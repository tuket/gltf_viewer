#pragma once

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

FilterCubemapError filterCubemap(const char* inTexFileName, const char* outTexFileName,
    const int w, const int h, const glm::vec2 (&facesCoords)[6]);


void cylinderMapToCubeMap(CubeImgView3f cube, CImg3f cylindricMap);

}

#include <stdio.h>
#include <tl/str.hpp>
#include <tg/texture_utils.hpp>
#include <stb/stbi.h>

using glm::vec2;
using glm::ivec2;

/*
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

bool test_cylinderMapToCubeMap()
{
    auto img = tg::Img3f::load("mercator.jpg");
    if(!img.data())
        return false;
    const int faceSize = img.height()/2;
    tg::Img3f cubeImg(4*faceSize, 3*faceSize);
    const ivec2 cubeFacesCoords[6] = {
        {0*faceSize, 1*faceSize}, // LEFT
        {2*faceSize, 1*faceSize}, // RIGHT
        {1*faceSize, 2*faceSize}, // DOWN
        {1*faceSize, 0*faceSize}, // UP
        {1*faceSize, 1*faceSize}, // FRONT
        {3*faceSize, 1*faceSize}  // BACK
    };
    auto cubeImgView = tg::CubeImgView3f::createFromSingleImg(cubeImg, faceSize, cubeFacesCoords);
    tg::cylinderMapToCubeMap(cubeImgView, img);
    cubeImg.save("output_cube_map.png");
    return true;
}

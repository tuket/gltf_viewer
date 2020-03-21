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
    tg::Img3f cubeImg(img.width(), img.height());
    const int cubeImgSize = img.height()/2;
    const ivec2 cubeFacesCoords[6] = {
        {0,               cubeImgSize/3   }, // LEFT
        {cubeImgSize/2,   cubeImgSize/3   }, // RIGHT
        {cubeImgSize/4,   cubeImgSize*2/3 }, // DOWN
        {cubeImgSize/4,   0               }, // UP
        {cubeImgSize/4,   cubeImgSize/3   }, // FRONT
        {cubeImgSize*3/4, cubeImgSize/3   }  // BACK
    };
    auto cubeImgView = tg::CubeImgView3f::createFromSingleImg(cubeImg, cubeImgSize, cubeFacesCoords);
    tg::cylinderMapToCubeMap(cubeImgView, img);
    cubeImg.save();
    return true;
}

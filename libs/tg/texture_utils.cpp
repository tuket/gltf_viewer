#include "texture_utils.hpp"

#include <tl/containers/table.hpp>
#include <glm/vec3.hpp>
#include <glm/glm.hpp>
#include <stb/stbi.h>
#include "geometry_utils.hpp"
#include <tl/basic.hpp>

using glm::vec2;
using glm::vec3;

namespace tg
{

FilterCubemapError filterCubemap(const char* inTexFileName, const char* outTexFileName,
    const int w, const int h, const vec2 (&facesCoords)[6])
{
    int mW, mH, mC;
    stbi_loadf(inTexFileName, &mW, &mH, &mC, 3);


    tl::Table<vec3> img(2*w, 2*h);

    return FilterCubemapError::NONE;
}

vec3 sampleImgQuad(CImg3f img, tl::CSpan<vec2> q)
{
    float x0f = q[0].x;
    for(size_t i = 0; i < q.size(); i++)
        x0f = tl::min(x0f, q[i].x);
    const int x0 = (int)floor(x0f);

    float y0f = q[0].y;
    for(size_t i = 0; i< q.size(); i++)
        y0f = tl::min(y0f, q[i].y);
    const int y0 = (int)floor(y0f);

    float x1f = q[0].x;
    for(size_t i = 0; i < q.size(); i++)
        x1f = tl::max(x1f, q[i].x);
    const int x1 = (int)ceil(x1f);

    float y1f = q[0].y;
    for(size_t i = 0; i < q.size(); i++)
        y1f = tl::max(y1f, q[i].y);
    const int y1 = (int)ceil(y1f);

    vec3 avg(0);
    for(int y = y0; y < y1; y++)
    for(int x = x0; x < x1; x++)
    {
        const float area = intersectionArea_square_quad(tl::rect(x, y, x+1, y+1), q);
        avg += area * img(x, y);
    }
    avg /= (x1-x0) * (y1-y0);
    return avg;
}

void cylinderMapToCubeMap(CubeImgView3f cube, CImg3f cylindricMap)
{
    // rays for each corner of the pixel
    auto calcFacePixelRays = [](vec3 (&rays)[4], ECubeImgFace eFace, float s05, int x, int y)
    {
        switch (eFace)
        {
        case ECubeImgFace::LEFT:
            rays[0] = {-1, (y-s05)/s05,   (s05-x)/s05};
            rays[1] = {-1, (y-s05)/s05,   (s05-x-1)/s05};
            rays[2] = {-1, (y-s05+1)/s05, (s05-x-1)/s05};
            rays[3] = {-1, (y-s05+1)/s05, (s05-x)/s05};
            break;
        case ECubeImgFace::RIGHT:
            rays[0] = {1, (y-s05)/s05,   (x-s05)/s05};
            rays[1] = {1, (y-s05)/s05,   (x-s05+1)/s05};
            rays[2] = {1, (y-s05+1)/s05, (x-s05+1)/s05};
            rays[3] = {1, (y-s05+1)/s05, (x-s05)/s05};
            break;
        case ECubeImgFace::DOWN:
            rays[0] = {(x-s05)/s05,   -1, (s05-y)/s05};
            rays[1] = {(x-s05+1)/s05, -1, (s05-y)/s05};
            rays[2] = {(x-s05+1)/s05, -1, (s05-y-1)/s05};
            rays[3] = {(x-s05)/s05,   -1, (s05-y-1)/s05};
            break;
        case ECubeImgFace::UP:
            rays[0] = {(x-s05)/s05,   1, (y-s05)/s05};
            rays[1] = {(x-s05+1)/s05, 1, (y-s05)/s05};
            rays[2] = {(x-s05+1)/s05, 1, (y-s05+1)/s05};
            rays[3] = {(x-s05)/s05,   1, (y-s05+1)/s05};
            break;
        case ECubeImgFace::FRONT:
            rays[0] = {(x-s05)/s05,   (y-s05)/s05,   1};
            rays[1] = {(x+1-s05)/s05, (y-s05)/s05,   1};
            rays[2] = {(x+1-s05)/s05, (y+1-s05)/s05, 1};
            rays[3] = {(x-s05)/s05,   (y+1-s05)/s05, 1};
            break;
        case ECubeImgFace::BACK:
            rays[0] = {(x-s05)/s05,   (y-s05)/s05,   -1};
            rays[1] = {(x+1-s05)/s05, (y-s05)/s05,   -1};
            rays[2] = {(x+1-s05)/s05, (y+1-s05)/s05, -1};
            rays[3] = {(x-s05)/s05,   (y+1-s05)/s05, -1};
            break;
        }
    };

    const float s05 = 0.5f * cube.sidePixels;
    for(int faceInd = 0; faceInd < 6; faceInd++)
    {
        auto eFace = (ECubeImgFace)faceInd;
        for(int y = 0; y < cube.sidePixels; y++)
        for(int x = 0; x < cube.sidePixels; x++)
        {
            vec3 rays[4]; // rays for each corner of the pixel
            calcFacePixelRays(rays, eFace, s05, x, y);

            vec2 texCoords[4];
            for(int i = 0; i < 4; i++) {
                vec3& r = rays[i];
                r = glm::normalize(r); // project onto unit sphere
                texCoords[i] = { r.y, glm::normalize(vec2{r.x, r.z}).x }; // project onto cylinder
                texCoords[i] = 0.5f * (texCoords[i] + vec2(1)) * vec2(cylindricMap.width(), cylindricMap.height());
            }
            cube[eFace](x, y) = sampleImgQuad(cylindricMap, texCoords);
        }
    }
}

}

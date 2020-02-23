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

vec3 sampleImgQuad(CImg3f img, tl::CArray<vec2>& q)
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
    auto doFace = [m = cylindricMap](int faceInd, vec2 )
    {

    };
    // front
    auto& face = cube.front;
    const int w = face.width();
    const int h = face.height();
    const float w05 = 0.5f * w;
    const float h05 = 0.5f * h;
    for(int y = 0; y < h; y++)
    for(int x = 0; x < w; x++)
    {
        vec3 rays[4] = { // rays for each corner of the pixel
            {(x-w05)/w05, (y-h05)/h05, 1},
            {(x+1-w05)/w05, (y-h05)/h05, 1},
            {(x+1-w05)/w05, (y+1-h05)/h05, 1},
            {(x-w05)/w05, (y+1-h05)/h05, 1},
        };
        vec2 texCoords[4] = {

        };
        for(int i = 0; i < 4; i++) {
            vec3& r = rays[i];
            r = glm::normalize(r); // project onto unit sphere
            texCoords[i] = { r.y, glm::normalize(vec2{r.x, r.z}).x }; // project onto cylinder
            texCoords[i] = 0.5f * (texCoords[i] + vec2(1)) * vec2(w, h);
        }
    }
}

}

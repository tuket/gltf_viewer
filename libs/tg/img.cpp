#include "img.hpp"

#include <stb/stbi.h>

using glm::vec3;
using glm::vec4;

namespace tg
{

template <int NUM_CHANNELS>
static void loadImgFloatN(const char* fileName,
    glm::vec<NUM_CHANNELS, float, glm::highp>*& dataPtr, int& stride, int& w, int& h)
{
    constexpr int pixelSize = NUM_CHANNELS*sizeof(float);
    static_assert(sizeof(glm::vec3) == pixelSize, "The compiler is adding some padding");
    tg::Img3f img;
    int channels;
    float* data = stbi_loadf(fileName, &w, &h, &channels, NUM_CHANNELS);
    stride = w;
    dataPtr = reinterpret_cast<glm::vec3*>(data);
}

template <>
Img<vec3> Img<glm::vec3>::load(const char* fileName)
{
    Img<vec3> img;
    loadImgFloatN(fileName, img._data, img._stride, img._w, img._h);
    return img;
}

}

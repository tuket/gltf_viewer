#include "img.hpp"

#include <stb/stbi.h>
#include <stb/stb_image_write.h>

using glm::vec3;
using glm::vec4;

namespace tg
{

static_assert(sizeof(glm::vec2) == 2*sizeof(float), "The compiler is adding some padding");
static_assert(sizeof(glm::vec3) == 3*sizeof(float), "The compiler is adding some padding");

template <int NUM_CHANNELS>
static void loadImgFloatN(const char* fileName,
    glm::vec<NUM_CHANNELS, float, glm::highp>*& dataPtr, int& stride, int& w, int& h)
{
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

template <>
bool Img<vec3>::save(const char* fileName, int quality)
{
    const char* ext = nullptr;
    for(const char* c = fileName; *c; c++)
        if(*c == '.')
            ext = c;
    if(!ext)
        return false;
    ext++;
    int okay = 0;
    switch (ext[0])
    {
        case 'b':
            okay = stbi_write_bmp(fileName, _w, _h, 3, _data);
            break;
        case 'p':
            okay = stbi_write_png(fileName, _w, _h, 3, _data, _stride*sizeof(vec3));
            break;
        case 'j':
            okay = stbi_write_jpg(fileName, _w, _h, 3, _data, quality);
            break;
        case 't':
            okay = stbi_write_tga(fileName, _w, _h, 3, _data);
            break;
        case 'h':
            okay = stbi_write_hdr(fileName, _w, _h, 2, (float*)_data);
            break;
    }
    return okay;
}

}

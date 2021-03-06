#include "img.hpp"

#include <stb/stbi.h>
#include <stb/stb_image_write.h>
#include <math.h>

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

template <int NUM_CHANNELS>
static int saveImgFloatN(const char* fileName, const void* data, int w, int h, int stride, int quality)
{
    const char* ext = nullptr;
    for(const char* c = fileName; *c; c++)
        if(*c == '.')
            ext = c;
    if(!ext)
        return false;
    ext++;
    auto dataF = (const float*)data;
    u8* dataU8 = nullptr;
    if(ext[0] == 'b' || ext[0] == 'p' || ext[0] == 'j' || ext[0] == 't') {
        dataU8 = new u8[w*h*NUM_CHANNELS];
        const float invGamma = 1.f / 2.2f;
        int i = 0;
        for(int y = 0; y < h; y++)
        for(int x = 0; x < w; x++)
        for(int c = 0; c < NUM_CHANNELS; c++, i++)
            dataU8[i] = (u8)(255 * pow(dataF[i], invGamma));
    }
    int okay = 0;
    switch (ext[0])
    {
        case 'b':
            okay = stbi_write_bmp(fileName, w, h, NUM_CHANNELS, dataU8);
            break;
        case 'p':
            okay = stbi_write_png(fileName, w, h, NUM_CHANNELS, dataU8, stride*sizeof(u8)*NUM_CHANNELS);
            break;
        case 'j':
            okay = stbi_write_jpg(fileName, w, h, NUM_CHANNELS, dataU8, quality);
            break;
        case 't':
            okay = stbi_write_tga(fileName, w, h, NUM_CHANNELS, dataU8);
            break;
        case 'h':
            okay = stbi_write_hdr(fileName, w, h, NUM_CHANNELS, dataF);
            break;
    }
    delete[] dataU8;
    return okay;
}

template <>
bool Img<vec3>::save(const char* fileName, int quality)
{
    return saveImgFloatN<3>(fileName, _data, _w, _h, _stride, quality);
}

}

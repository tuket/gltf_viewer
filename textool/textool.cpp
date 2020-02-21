#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stbi.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <glm/glm.hpp>
#include <tl/carray.hpp>
#include <tl/containers/fvector.hpp>
#include <tl/basic.hpp>
#include <tl/basic_math.hpp>

using glm::vec2;
using glm::vec3;

struct Rect { vec2 pMin, pMax; };

enum class FilterCubemapError {
    NONE,
    CANT_OPEN_INPUT_FILE,
    CANT_OPEN_OUTPUT_FILE,
};

static FilterCubemapError filterCubemap(const char* inTexFileName, const char* outTexFileName,
    const int w, const int h, const vec2 (&facesCoords)[6]);

class Img
{
public:
    Img(int w, int h) : w(w), h(h), data(new vec3[3*w*h]) {}
    ~Img() { delete[] data; }
    vec3& operator()(int x, int y) { return data[x + w*y]; }
    vec3* operator[](int y) { return data + w*y; }
    int getW()const { return w; }
    int getH()const { return h; }
private:
    int w, h;
    vec3* data;
};

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

static FilterCubemapError filterCubemap(const char* inTexFileName, const char* outTexFileName,
    const int w, const int h, const vec2 (&facesCoords)[6])
{
    int mW, mH, mC;
    stbi_loadf(inTexFileName, &mW, &mH, &mC, 3);


    Img img(2*w, 2*h);

    return FilterCubemapError::NONE;
}

void cylinderMapToCubeMap(Img (&img)[6], const Img& cylindricMap)
{

}

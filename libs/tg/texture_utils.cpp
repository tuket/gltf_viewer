#include "texture_utils.hpp"

#include <tl/containers/table.hpp>
#include <glm/vec3.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <stb/stbi.h>
#include "geometry_utils.hpp"
#include "shader_utils.hpp"
#include <tl/basic.hpp>
#include <tl/basic_math.hpp>
#include <glad/glad.h>
#include <string.h>

using glm::vec2;
using glm::vec3;

constexpr float PI = glm::pi<float>();
constexpr float PI2 = 2*PI;

namespace tg
{

// --- DATA ---------------------------------------------------------------------------------------

static const char s_glslVersionSrc[] = "#version 330 core\n\n";

static const char s_filterCubemapSrc_vertShad[] =
R"GLSL(
)GLSL";

static const char s_hammersleyShadSrc[] =
R"GLSL(
vec2 hammersley(uint i, uint numSamples)
{
    b = (i << 16u) | (i >> 16u);
    b = ((i & 0x55555555u) << 1u) | ((i & 0xAAAAAAAAu) >> 1u);
    b = ((i & 0x33333333u) << 2u) | ((i & 0xCCCCCCCCu) >> 2u);
    b = ((i & 0x0F0F0F0Fu) << 4u) | ((i & 0xF0F0F0F0u) >> 4u);
    b = ((i & 0x00FF00FFu) << 8u) | ((i & 0xFF00FF00u) >> 8u);
    float radicalInverseVDC = float(i) * 2.3283064365386963e-10;
    return vec2(float(i) / float(numSamples), radicalInverseVDC);
}
)GLSL";

static const char s_ggxShadSrc[] =
R"GLSL(
vec3 generateDirection(vec3 rayDir, float rough2, uint i)
{

}
)GLSL";

static const char s_filterCubemapSrc_vars[] =
R"GLSL(
layout(location = 0) out vec4 o_color;

in vec3 a_rayDir;

uniform samplerCube u_cubemap;
uniform uint u_numSamples;
uniform float u_rough2; // roughness squared
)GLSL";

static const char s_filterCubemapSrc_main[] =
R"GLSL(
void main()
{
    vec3 outColor = vec3(0.0, 0.0, 0.0);
    for(uint i = 0; i < u_numSamples; i++)
    {
        vec3 dir = generateDirection();
        outColor += texture(u_cubemap, u_rough2, i);
    }
    outColor /= float(numSamples);
    o_color = outColor;
}
)GLSL";

static char s_buffer[4*1024];

static float s_cubeVerts[6*6*(3+2)] = {
    // x, y, z,  u,     v
    // LEFT
    -1, -1, -1,  0,     1/3.f,
    -1, -1, +1,  1/4.f, 1/3.f,
    -1, +1, +1,  1/4.f, 2/3.f,
    -1, -1, -1,  0,     1/3.f,
    -1, +1, +1,  1/4.f, 2/3.f,
    -1, +1, -1,  0,     2/3.f,
    // RIGHT
    +1, -1, +1,  2/4.f, 1/3.f,
    +1, -1, -1,  3/4.f, 1/3.f,
    +1, +1, -1,  3/4.f, 2/3.f,
    +1, -1, +1,  2/4.f, 1/3.f,
    +1, +1, -1,  3/4.f, 2/3.f,
    +1, +1, +1,  2/4.f, 2/3.f,
    // BOTTOM
    -1, -1, -1,  1/4.f, 0,
    +1, -1, -1,  2/4.f, 0,
    +1, -1, +1,  2/4.f, 1/3.f,
    -1, -1, -1,  1/4.f, 0,
    +1, -1, +1,  2/4.f, 1/3.f,
    -1, -1, +1,  1/4.f, 1/3.f,
    // TOP
    -1, +1, +1,  1/4.f, 2/3.f,
    +1, +1, +1,  2/4.f, 2/3.f,
    +1, +1, -1,  2/4.f, 1,
    -1, +1, +1,  1/4.f, 2/3.f,
    +1, +1, -1,  2/4.f, 1,
    -1, +1, -1,  1/4.f, 1,
    // FRONT
    -1, -1, +1,  1/4.f, 1/3.f,
    +1, -1, +1,  2/4.f, 1/3.f,
    +1, +1, +1,  2/4.f, 2/3.f,
    -1, -1, +1,  1/4.f, 1/3.f,
    +1, +1, +1,  2/4.f, 2/3.f,
    -1, +1, +1,  1/4.f, 2/3.f,
    // BACK
    +1, -1, -1,  3/4.f, 1/3.f,
    -1, -1, -1,  1,     1/3.f,
    -1, +1, -1,  1,     2/3.f,
    +1, -1, -1,  3/4.f, 1/3.f,
    -1, +1, -1,  1,     2/3.f,
    +1, +1, -1,  3/4.f, 2/3.f
};

// --- CODE -------------------------------------------------------------------------------------------------------------

u32 createFilterCubemapVertShader()
{
    const u32 shad = glCreateShader(GL_VERTEX_SHADER);
    const char* srcs[] = {s_glslVersionSrc, s_filterCubemapSrc_vertShad};
    constexpr int numSrcs = tl::size(srcs);
    i32 sizes[numSrcs];
    for(int i = 0; i < numSrcs; i++)
        sizes[i] = strlen(srcs[i]);
    glShaderSource(shad, numSrcs, srcs, sizes);
    glCompileShader(shad);
    if(const char* errorMsg = getShaderCompileErrors(shad, s_buffer))
    {
        fprintf(stderr, "Error compiling(createFilterCubemapVertShader): %s\n", errorMsg);
        glDeleteShader(shad);
        return 0;
    }
    return shad;
}

u32 createFilterCubemap_ggx_fragShader()
{
    const u32 shad = glCreateShader(GL_FRAGMENT_SHADER);
    const char* srcs[] = {
        s_glslVersionSrc, s_filterCubemapSrc_vars,
        s_hammersleyShadSrc,
        s_ggxShadSrc,
        s_filterCubemapSrc_main
    };
    constexpr int numSrcs = tl::size(srcs);
    int sizes[numSrcs];
    for(int i = 0; i < numSrcs; i++)
        sizes[i] = strlen(srcs[i]);
    glShaderSource(shad, numSrcs, srcs, sizes);
    glCompileShader(shad);
    if(const char* errorMsg = getShaderCompileErrors(shad, s_buffer))
    {
        fprintf(stderr, "Error compiling(createFilterCubemapFragShader_GGX): \n%s", errorMsg);
        glDeleteShader(shad);
        return 0;
    }
    return shad;
}

GgxFilterUnifLocs getFilterCubamap_ggx_unifLocs(u32 prog)
{
    GgxFilterUnifLocs locs;
    locs.cubemap = glGetUniformLocation(prog, "u_cubemap");
    locs.numSamples = glGetUniformLocation(prog, "u_numSamples");
    locs.roughness2 = glGetUniformLocation(prog, "u_rough2");
    assert(locs.cubemap != -1 && locs.numSamples != -1 && locs.roughness2 != -1);
    return locs;
}

void createCubemapMeshGpu(u32& vao, u32& vbo, u32& numVerts)
{
    numVerts = tl::size(s_cubeVerts) / 5;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_cubeVerts), s_cubeVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3*sizeof(float), GL_FLOAT, GL_FALSE, 5*sizeof(float), nullptr);
    glVertexAttribPointer(1, 2*sizeof(float), GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(3*sizeof(float)));
}

void filterCubemap_GGX(tl::FVector<Img3f, 16>& outMips,
    ImgView3f inImg, u32 cubemapMeshVao,
    u32 shaderProg, const GgxFilterUnifLocs& locs)
{
    constexpr int numSamples = 16;
    const int sidePixels = tl::min(inImg.width() / 4, inImg.height() / 3);
    int numMips = 0;
    for(int px = sidePixels/2; px; px /= 2)
        numMips++;
    assert(numMips <= 16);

    u32 inTex;
    glGenTextures(1, &inTex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, inImg.width(), inImg.height(), 0, GL_RGB, GL_FLOAT, inImg.data());

    glBindVertexArray(cubemapMeshVao);
    glUseProgram(shaderProg);
    glUniform1i(locs.cubemap, 0);
    glUniform1i(locs.numSamples, numSamples);

    glActiveTexture(GL_TEXTURE1);
    u32 framebuffer;
    u32 textures[16];
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glGenTextures(numMips, textures);
    int mipSidePixels = sidePixels;
    for(int iMip = 0; iMip < numMips; iMip++)
    {
        const float roughness = float(numMips-iMip) / numMips;
        const float roughness2 = roughness * roughness;
        glUniform1f(locs.roughness2, roughness2);
        u32& tex = textures[iMip];
        glBindTexture(GL_TEXTURE_2D, tex);
        mipSidePixels /= 2;
        const int w = 4 * mipSidePixels;
        const int h = 3 * mipSidePixels;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);
        struct FaceZone {i32 x, y, w, h;};
        static const FaceZone faceZones[] = {
            {0*sidePixels, 1*sidePixels, sidePixels, sidePixels}, // LEFT
            {2*sidePixels, 1*sidePixels, sidePixels, sidePixels}, // RIGHT
            {1*sidePixels, 2*sidePixels, sidePixels, sidePixels}, // DOWN
            {1*sidePixels, 0*sidePixels, sidePixels, sidePixels}, // UP
            {1*sidePixels, 1*sidePixels, sidePixels, sidePixels}, // FRONT
            {3*sidePixels, 1*sidePixels, sidePixels, sidePixels}, // BACK
        };
        for(int i = 0; i < 6; i++)
        {
            const FaceZone& fz = faceZones[i];
            glViewport(fz.x, fz.y, fz.w, fz.h);
            glScissor(fz.x, fz.y, fz.w, fz.h);
            const auto eFace = (ECubeImgFace)i;
        }
    }
    glDeleteTextures(numMips, textures);
    glDeleteFramebuffers(1, &framebuffer);
}

FilterCubemapError filterCubemap_GGX(const char* inImgFileName,
    const char* outImgFileNamePrefix, const char* outImgExtension,
    u32 shaderProg, u32 cubemapMesh)
{
    Img3f inImg = Img3f::load(inImgFileName);
    if(!inImg.data())
        return FilterCubemapError::CANT_OPEN_INPUT_FILE;

    const int w = inImg.width();
    const int h = inImg.height();
    const int sidePixelsAcordingToW = w / 4;
    const int sidePixelsAccordingToH = h / 3;
    if(sidePixelsAcordingToW != sidePixelsAccordingToH) {
        fprintf(stderr, "%s has a weird aspect ratio for a cubemap\n", inImgFileName);
    }
    const int sidePixels = tl::min(sidePixelsAcordingToW, sidePixelsAccordingToH);
    CubeImgView3f inCubeView;
    inCubeView.sidePixels = sidePixels;

    return FilterCubemapError::NONE;
}

/* The points in q must be in counter-clock-wise order */
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
        assert(area >= 0.f && area < 1.001f);
        assert(y < img.height());
        avg += area * img(x % img.width(), y);
    }
    avg /= convexPolyArea(q);
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
            rays[0] = {-1, (y-s05)/s05,   (x-s05)/s05};
            rays[1] = {-1, (y-s05)/s05,   (x-s05+1)/s05};
            rays[2] = {-1, (y-s05+1)/s05, (x-s05+1)/s05};
            rays[3] = {-1, (y-s05+1)/s05, (x-s05)/s05};
            break;
        case ECubeImgFace::RIGHT:
            rays[0] = {1, (y-s05)/s05,   (s05-x)/s05};
            rays[1] = {1, (y-s05)/s05,   (s05-x-1)/s05};
            rays[2] = {1, (y-s05+1)/s05, (s05-x-1)/s05};
            rays[3] = {1, (y-s05+1)/s05, (s05-x)/s05};
            break;
        case ECubeImgFace::DOWN:
            rays[0] = {(x-s05)/s05,   1, (s05-y)/s05};
            rays[1] = {(x-s05+1)/s05, 1, (s05-y)/s05};
            rays[2] = {(x-s05+1)/s05, 1, (s05-y-1)/s05};
            rays[3] = {(x-s05)/s05,   1, (s05-y-1)/s05};
            break;
        case ECubeImgFace::UP:
            rays[0] = {(x-s05)/s05,   -1, (y-s05)/s05};
            rays[1] = {(x-s05+1)/s05, -1, (y-s05)/s05};
            rays[2] = {(x-s05+1)/s05, -1, (y-s05+1)/s05};
            rays[3] = {(x-s05)/s05,   -1, (y-s05+1)/s05};
            break;
        case ECubeImgFace::FRONT:
            rays[0] = {(x-s05)/s05,   (y-s05)/s05,   1};
            rays[1] = {(x+1-s05)/s05, (y-s05)/s05,   1};
            rays[2] = {(x+1-s05)/s05, (y-s05+1)/s05, 1};
            rays[3] = {(x-s05)/s05,   (y-s05+1)/s05, 1};
            break;
        case ECubeImgFace::BACK:
            rays[0] = {(s05-x)/s05,   (y-s05)/s05,   -1};
            rays[1] = {(s05-x-1)/s05, (y-s05)/s05,   -1};
            rays[2] = {(s05-x-1)/s05, (y-s05+1)/s05, -1};
            rays[3] = {(s05-x)/s05,   (y-s05+1)/s05, -1};
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
                // project onto cylinder
                float angle = atan2f(r.x, r.z) + PI;
                texCoords[i] = {angle / PI2, r.y};
                assert(texCoords[i].x >= 0);
                texCoords[i].x *= cylindricMap.width();
                texCoords[i].y = cylindricMap.height() * 0.5f * (texCoords[i].y + 1);
            }
            // here we make sure that that we don't "wrap around the sphere"
            // this happens when one ray of the pixel has and azimuth close to 0 of 360
            // and the other ray crosses the 360º fontier, that would produce a quad that cover most of the image
            if(texCoords[0].x - texCoords[1].x > 0.5f*cylindricMap.width() ||
               texCoords[3].x - texCoords[2].x > 0.5f*cylindricMap.width())
            {
                texCoords[1].x += cylindricMap.width();
                texCoords[2].x += cylindricMap.width();
            }
            else if(texCoords[1].x - texCoords[0].x > 0.5f*cylindricMap.width() ||
                    texCoords[2].x - texCoords[3].x > 0.5f*cylindricMap.width())
            {
                texCoords[0].x += cylindricMap.width();
                texCoords[3].x += cylindricMap.width();
            }

            cube[eFace](x, y) = sampleImgQuad(cylindricMap, texCoords);
        }
    }
}

}

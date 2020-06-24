#include "texture_utils.hpp"

#include <tl/containers/table.hpp>
#include <glm/vec3.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <stb/stbi.h>
#include "geometry_utils.hpp"
#include "shader_utils.hpp"
#include "mesh_utils.hpp"
#include <tl/basic.hpp>
#include <tl/basic_math.hpp>
#include <tl/defer.hpp>
#include <tl/fmt.hpp>
#include <glad/glad.h>
#include <string.h>

using glm::vec2;
using glm::vec3;

constexpr float PI = glm::pi<float>();
constexpr float PI2 = 2*PI;

namespace tg
{

// --- DATA ---------------------------------------------------------------------------------------

static const char s_filterFullscreenSrc_vertShad[] =
R"GLSL(
layout (location = 0) in vec3 a_pos;

void main()
{
    v_tc = 0.5 * (a_pos + 1.0;
    gl_Position = vec3(a_pos, 0.0, 1.0);
}
)GLSL";

static const char s_filterNothing_fragShad[] =
R"GLSL(
layout(location = 0) out vec4 o_color;

uniform sampler2D u_texture;

in vec3 v_tc;

void main()
{
    o_color = texture(u_cubemap, v_tc);
}
)GLSL";

static const char s_filterCubemapSrc_vertShad[] =
R"GLSL(
layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec2 a_tc;

out vec3 v_lobeDir;

void main()
{
    v_lobeDir = a_pos;
    gl_Position = vec4(vec2(2.0, -2.0)*a_tc-vec2(1.0, -1.0), 0.0, 1.0);
}
)GLSL";

static const char s_ggxShadSrc[] =
R"GLSL(
void main()
{
    vec3 N = normalize(v_lobeDir);
    vec3 outColor = vec3(0.0, 0.0, 0.0);
    for(uint i = 0u; i < u_numSamples; i++)
    {
        vec2 seed = hammersleyVec2(i, u_numSamples);
        vec3 h = importanceSampleGgxD(seed, u_rough2, N);
        vec3 l = 2.0 * dot(v_lobeDir, h) * h - v_lobeDir;

        vec3 sampleColor = texture(u_cubemap, l).rgb;
        float mysteriousFactor = max(0, dot(N, l)); // in the unreal paper, they say this factor improves results, but there is no explanation
        outColor += sampleColor /* * mysteriousFactor */;
    }
    outColor /= float(u_numSamples);
    o_color = vec4(outColor, 1.0);
}
)GLSL";

static const char s_filterCubemapSrc_vars[] =
R"GLSL(
layout(location = 0) out vec4 o_color;

in vec3 v_lobeDir;

uniform samplerCube u_cubemap;
uniform uint u_numSamples;
uniform float u_rough2; // roughness squared
)GLSL";

static const char s_simpleScreenQuadVertShadSrc[] =
R"GLSL(
layout(location = 0) in vec2 a_pos;

out vec2 v_tc;

void main()
{
    gl_Position = vec4(a_pos, 0, 1);
    v_tc = 0.5 * (a_pos + 1.0);
}
)GLSL";

static const char s_ggxLutGenerateSrc_fragShad[] =
R"GLSL(
layout(location = 0) out vec4 o_color;

uniform uint u_numSamples;

in vec2 v_tc;

float ggxG(float NoV, float rough4)
{
    return 2.0 * NoV /
        ( NoV + sqrt(rough4 + (1.0 - rough4) * NoV*NoV) );
}

void main()
{
    float NoV = v_tc.x;
    float rough2 = v_tc.y;
    vec3 V = vec3(
        sqrt(1.0 - NoV * NoV),
        0,
        NoV);
    float rough4 = rough2 * rough2;
    vec2 r = vec2(0);
    for(uint iSample = 0u; iSample < u_numSamples; iSample++)
    {
        vec2 seed = hammersleyVec2(iSample, u_numSamples);
        vec3 H = importanceSampleGgxD(seed, rough2, vec3(0, 0, 1));
        vec3 L = 2.0 * dot(V, H) * H - V;
        float NoL = max(0.0, L.z);
        float NoH = max(0.0, H.z);
        float VoH = max(0.0, dot(V, H));
        if(NoL > 0.0) {
            float G = ggxG(NoV, rough4) * ggxG(NoL, rough4);
            float G_vis = G * VoH / (NoH * NoV);
            float Fc = pow(1.0 - VoH, 5.0);
            r.x += (1 - Fc) * G_vis;
            r.y += Fc * G_vis;
        }
    }

    r /= float(u_numSamples);
    o_color = vec4(r, 0, 1);
}
)GLSL";

static char s_buffer[4*1024];

static float s_filterCubemapVerts[6*6*(3+2)] = {
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

u32 createFilterFullscreenVertShader()
{
    const u32 shad = glCreateShader(GL_VERTEX_SHADER);
    const char* srcs[] = {srcs::header, s_filterFullscreenSrc_vertShad};
    constexpr int numSrcs = tl::size(srcs);
    i32 sizes[numSrcs];
    for(int i = 0; i < numSrcs; i++)
        sizes[i] = strlen(srcs[i]);
    glShaderSource(shad, numSrcs, srcs, sizes);
    glCompileShader(shad);
    if(const char* errorMsg = getShaderCompileErrors(shad, s_buffer))
    {
        fprintf(stderr, "Error compiling(createFilterFullscreenVertShader): %s\n", errorMsg);
        glDeleteShader(shad);
        return 0;
    }
    return shad;
}

u32 createFilterCubemapVertShader()
{
    const u32 shad = glCreateShader(GL_VERTEX_SHADER);
    const char* srcs[] = {srcs::header, s_filterCubemapSrc_vertShad};
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

void createFilterCubemapMeshGpu(u32& vao, u32& vbo, u32& numVerts)
{
    numVerts = tl::size(s_filterCubemapVerts) / 5;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_filterCubemapVerts), s_filterCubemapVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float), nullptr);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)(3*sizeof(float)));
}

template <int numSrcs>
static u32 createFragShaderCommon(const char* (&srcs)[numSrcs], const char* shaderName)
{
    const u32 shad = glCreateShader(GL_FRAGMENT_SHADER);
    int sizes[numSrcs];
    for(int i = 0; i < numSrcs; i++)
        sizes[i] = strlen(srcs[i]);
    glShaderSource(shad, numSrcs, srcs, sizes);
    glCompileShader(shad);
    if(const char* errorMsg = getShaderCompileErrors(shad, s_buffer))
    {
        fprintf(stderr, "Error compiling(%s): \n%s", shaderName, errorMsg);
        glDeleteShader(shad);
        return 0;
    }
    return shad;
}

u32 createFilterNothingFragShader()
{
    const char* srcs[] = {
        srcs::header,
        s_filterNothing_fragShad,
    };
    return createFragShaderCommon(srcs, "downscaleFragShader");
}

u32 createFilterCubemap_ggx_fragShader()
{
    const char* srcs[] = {
        srcs::header,
        s_filterCubemapSrc_vars,
        srcs::hammersley,
        srcs::importanceSampleGgxD,
        s_ggxShadSrc,
    };
    return createFragShaderCommon(srcs, "downscaleFragShader");
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

u8 getNumChannels(u32 format)
{
    switch(format)
    {
    case GL_RED:
        return 1;
    case GL_RG:
        return 2;
    case GL_RGB:
    case GL_BGR:
            return 3;
    case GL_RGBA:
    case GL_BGRA:
        return 4;
    // There are many more but I'm lazy
    }
    assert(false);
    return 0;
}

u8 getGetPixelSize(u32 format, u32 type)
{
    const u32 nc = getNumChannels(format);
    switch(type)
    {
    case GL_UNSIGNED_BYTE:
    case GL_BYTE:
        return nc;
    case GL_UNSIGNED_SHORT:
    case GL_SHORT:
    case GL_HALF_FLOAT:
        return 2*nc;
    case GL_UNSIGNED_INT:
    case GL_INT:
    case GL_FLOAT:
        return 4*nc;
    // there are many more but I'm lazy
    }
    assert(false);
    return 1;
}

void simpleInitCubemapTexture()
{
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

void uploadCubemapTexture(u32 mipLevel, u32 w, u32 h, u32 internalFormat, u32 dataFormat, u32 dataType, u8* data)
{
    const u8 ps = getGetPixelSize(dataFormat, dataType);
    const u32 side = w / 4;
    assert(3*side == h);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, w);
    defer(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));
    auto upload = [&](GLenum face, u32 offset) {
        glTexImage2D(face, mipLevel, internalFormat, side, side, 0, dataFormat, dataType, data + offset);
    };
    upload(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, ps * w*side);
    upload(GL_TEXTURE_CUBE_MAP_POSITIVE_X, ps * (w*side + 2*side));
    upload(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, ps * (w*2*side + side));
    upload(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, ps * side);
    upload(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, ps * (w*side + 3*side));
    upload(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, ps * (w*side + side));
}

u32 downscaledTexture(u32 inTex, u32 inW, u32 inH, u32 outW, u32 outH)
{

}

void createGgxLutTexShader(u32& prog, u32& vertShad, u32& fragShad, u32& numSamplesUnifLoc)
{
    const char* vertSrcs[] = {
        srcs::header,
        s_simpleScreenQuadVertShadSrc
    };
    const char* fragSrcs[] = {
        srcs::header,
        srcs::hammersley,
        srcs::importanceSampleGgxD,
        s_ggxLutGenerateSrc_fragShad
    };

    constexpr i32 numVertSrcs = tl::size(vertSrcs);
    constexpr i32 numFragSrcs = tl::size(fragSrcs);
    i32 strLengths[tl::max(numVertSrcs, numFragSrcs)];

    vertShad = glCreateShader(GL_VERTEX_SHADER);
    defer(glDeleteShader(vertShad));
    for(i32 i = 0; i < numVertSrcs; i++)
        strLengths[i] = tl::strlen(vertSrcs[i]);
    glShaderSource(vertShad, numVertSrcs, vertSrcs, strLengths);
    glCompileShader(vertShad);
    if(const char* errMsg = tg::getShaderCompileErrors(vertShad, s_buffer)) {
        tl::println("Error compiling vertex shader:\n", errMsg);
        assert(false);
    }

    fragShad = glCreateShader(GL_FRAGMENT_SHADER);
    defer(glDeleteShader(fragShad));
    for(i32 i = 0; i < numFragSrcs; i++)
        strLengths[i] = tl::strlen(fragSrcs[i]);
    glShaderSource(fragShad, numFragSrcs, fragSrcs, strLengths);
    glCompileShader(fragShad);
    if(const char* errMsg = tg::getShaderCompileErrors(fragShad, s_buffer)) {
        tl::println("Error compiling fragment shader:\n", errMsg);
        assert(false);
    }

    prog = glCreateProgram();
    defer(glDeleteProgram(prog));
    glAttachShader(prog, vertShad);
    glAttachShader(prog, fragShad);
    glLinkProgram(prog);
    if(const char* errMsg = tg::getShaderLinkErrors(prog, s_buffer)) {
        tl::println("Error linking shader:\n", errMsg);
        assert(false);
    }

    glUseProgram(prog);
    numSamplesUnifLoc = glGetUniformLocation(prog, "u_numSamples");
}

Img3f generateGgxLutImg(u32 size)
{
    Img3f img;

    u32 prog, vertShad, fragShad, numSamplesUnifLoc;
    createGgxLutTexShader(prog, vertShad, fragShad, numSamplesUnifLoc);
    glUniform1ui(numSamplesUnifLoc, 128);
    defer(
        glDeleteShader(vertShad);
        glDeleteShader(fragShad);
        glDeleteProgram(prog);
    );

    u32 vao, vbo, numVerts;
    tg::createScreenQuadMesh2D(vao, vbo, numVerts);
    glBindVertexArray(vao);
    defer(
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
    );

    u32 fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    u32 tex;
    glGenTextures(1, &tex);
    defer(glDeleteTextures(1, &tex));
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size, size, 0, GL_RGB, GL_FLOAT, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);


    glViewport(0, 0, size, size);
    glScissor(0, 0, size, size);
    glDrawArrays(GL_TRIANGLES, 0, numVerts);
    img = Img3f(size, size);
    glReadPixels(0, 0, size, size, GL_RGB, GL_FLOAT, img.data());

    return img;
}



}

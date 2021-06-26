#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stbi.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <tl/basic.hpp>
#include <tl/basic_math.hpp>
#include <tl/fmt.hpp>
#include <tl/defer.hpp>
#include <tg/texture_utils.hpp>
#include <tg/shader_utils.hpp>
#include <tg/mesh_utils.hpp>

static char s_buffer[4*1024];

static const char* getGlErrStr(GLenum const err)
{
    switch (err) {
    case GL_NO_ERROR: return "GL_NO_ERROR";
    case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
    case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
#ifdef GL_STACK_UNDERFLOW
    case GL_STACK_UNDERFLOW: return "GL_STACK_UNDERFLOW";
#endif
#ifdef GL_STACK_OVERFLOW
    case GL_STACK_OVERFLOW: return "GL_STACK_OVERFLOW";
#endif
    default:
        assert(!"unknown error");
        return nullptr;
    }
}

static void glErrorCallback(const char *name, void *funcptr, int len_args, ...)
{
    GLenum error_code;
    error_code = glad_glGetError();
    if (error_code != GL_NO_ERROR) {
        fprintf(stderr, "ERROR %s in %s\n", getGlErrStr(error_code), name);
        assert(false);
    }
}

static GLFWwindow* initGlfwGL()
{
    glfwSetErrorCallback(+[](int error, const char* description) {
        fprintf(stderr, "Glfw Error %d: %s\n", error, description);
    });
    if (!glfwInit())
        return nullptr;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    //glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "test", nullptr, nullptr);
    if (window == nullptr)
        return nullptr;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // Enable vsync

    if (gladLoadGL() == 0) {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return nullptr;
    }
    glad_set_post_callback(glErrorCallback);
    return window;
}

void initTexture(int width, int height, void* data) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_FLOAT, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
};

bool filterCubemap()
{
    auto window = initGlfwGL();
    defer(glfwTerminate());

    // load the image
    auto inImg = tg::Img3f::load("autumn_cube.hdr");
    u32 inTex;
    glGenTextures(1, &inTex);
    defer(glDeleteTextures(1, &inTex));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inTex);
    tg::simpleInitCubemapTexture();
    int sidePixels = inImg.width() / 4;
    assert(sidePixels == inImg.height() / 3);
    int w = 4 * sidePixels;
    int h = 3 * sidePixels;
    initTexture(w, h, inImg.data());

    // looks like using the full resolution texture as input to the ggx filter does not improve quality
    /*u32 inTex_cube;
    glGenTextures(1, &inTex_cube);
    glBindTexture(GL_TEXTURE_CUBE_MAP, inTex_cube);
    tg::simpleInitCubemapTexture();
    tg::uploadCubemapTexture(0, w, h, GL_RGB, GL_RGB, GL_FLOAT, (u8*)inImg.data());*/

    const u32 filterNothingProg = glCreateProgram();
    tg::FilterNothingUnifLocs filterNothinLocs;
    {
        const u32 filterVertShad = tg::createFilterVertShader();
        defer(glDeleteShader(filterVertShad));
        const u32 filterNothingFragShad = tg::createFilterNothingFragShader();
        defer(glDeleteShader(filterNothingFragShad));
        glAttachShader(filterNothingProg, filterVertShad);
        glAttachShader(filterNothingProg, filterNothingFragShad);
        glLinkProgram(filterNothingProg);
        if(const char* errMsg = tg::getShaderLinkErrors(filterNothingProg, s_buffer)) {
            tl::println("Error linking shader:\n", errMsg);
            assert(false);
        }
        tg::getFilterNothingUnifLocs(filterNothinLocs, filterNothingProg);
    }
    defer(glDeleteProgram(filterNothingProg));

    const u32 ggxFilterProg = glCreateProgram();
    {
        const u32 vertShader = tg::createFilterCubemapVertShader();
        defer(glDeleteShader(vertShader));
        const u32 fragShader = tg::createFilterCubemap_ggx_fragShader();
        defer(glDeleteShader(fragShader));
        glAttachShader(ggxFilterProg, vertShader);
        glAttachShader(ggxFilterProg, fragShader);
        glLinkProgram(ggxFilterProg);
        if(const char* errMsg = tg::getShaderLinkErrors(ggxFilterProg, s_buffer)) {
            tl::println("Error linking shader:\n", errMsg);
            assert(false);
        }
    }
    defer(glDeleteProgram(ggxFilterProg));
    const tg::GgxFilterUnifLocs ggxFilterUnifLocs = tg::getFilterCubamap_ggx_unifLocs(ggxFilterProg);

    u32 cubeVao, cubeVbo, cubeNumVerts;
    tg::createFilterCubemapMeshGpu(cubeVao, cubeVbo, cubeNumVerts);

    u32 quadVao, quadVbo, quadNumVerts;
    tg::createScreenQuadMesh2D(quadVao, quadVbo, quadNumVerts);

    u32 framebuffer;
    u32 rbo;
    glGenFramebuffers(1, &framebuffer);
    defer(glDeleteFramebuffers(1, &framebuffer));
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glGenRenderbuffers(1, &rbo);
    defer(glDeleteRenderbuffers(1, &rbo));
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB16F, 4*sidePixels, 3*sidePixels);
    glReadBuffer(GL_COLOR_ATTACHMENT0);

    glClearColor(0,0,0,1);

    auto nextDownSampledTex = [&](u32 inTex) -> u32
    {
        u32 outTex;
        glGenTextures(1, &outTex);
        glBindTexture(GL_TEXTURE_2D, outTex);
        sidePixels = (sidePixels+1) / 2;
        w = 4 * sidePixels;
        h = 3 * sidePixels;
        initTexture(w, h, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTex, 0);
        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
        glViewport(0,0, w,h);
        glScissor(0,0, w,h);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(filterNothingProg);
        glUniform1i(filterNothinLocs.texture, 0);
        glBindTexture(GL_TEXTURE_2D, inTex);
        glBindVertexArray(quadVao);
        glDrawArrays(GL_TRIANGLES, 0, quadNumVerts);
        return outTex;
    };

    auto createGgxFilteredTex = [&](tg::CImg3f img, float rough2, u32 numSamples) -> u32
    {
        u32 tmpTex;
        defer(glDeleteTextures(1, &tmpTex));
        glGenTextures(1, &tmpTex);
        defer(glDeleteTextures(1, &tmpTex));
        glBindTexture(GL_TEXTURE_CUBE_MAP, tmpTex);
        tg::simpleInitCubemapTexture();
        tg::uploadCubemapTexture(0, img.width(), img.height(), GL_RGB, GL_RGB, GL_FLOAT, (u8*)img.data());
        u32 outTex;
        glGenTextures(1, &outTex);
        glBindTexture(GL_TEXTURE_2D, outTex);
        initTexture(w, h, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTex, 0);
        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
        glViewport(0,0, w,h);
        glScissor(0,0, w,h);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindTexture(GL_TEXTURE_CUBE_MAP, tmpTex);
        glUseProgram(ggxFilterProg);
        glUniform1i(ggxFilterUnifLocs.cubemap, 0);
        glUniform1ui(ggxFilterUnifLocs.numSamples, numSamples);
        glUniform1f(ggxFilterUnifLocs.roughness2, rough2);
        glBindVertexArray(cubeVao);
        glDrawArrays(GL_TRIANGLES, 0, cubeNumVerts);
        return outTex;
    };

    struct StepConfig {
        i32 srcStep;
        u32 numSamples;
        float rough2;
        bool generateGgxFiltered;
    };
    const StepConfig steps[] = {
        {-1, 500, 0.01, true},
        {0, 1000, 0.025, true},
        {1, 1000, 0.05, true},
        {1, 10000, 0.1, true},
        {2, 10000, 0.3, true},
        {0, 0, 0, false},
        {0, 0, 0, false},
        {3, 10000, 1.0, true},
    };
    constexpr int numSteps = tl::size(steps);
    struct StepOutput {
        u32 dsTex, ggxTex;
        i32 w, h;
        tg::Img3f dsImg;
    };
    StepOutput stepOuts[numSteps];

    for(int i = 0; i < numSteps; i++)
    {
        stepOuts[i].dsTex = nextDownSampledTex(i ? stepOuts[i-1].dsTex : inTex);
        stepOuts[i].dsImg = tg::Img3f(w, h);
        glReadPixels(0, 0, w, h, GL_RGB, GL_FLOAT, stepOuts[i].dsImg.data());
        tl::toStringBuffer(s_buffer, "test_ds_", i+1, ".hdr");
        stepOuts[i].dsImg.save(s_buffer);

        stepOuts[i].w = w;
        stepOuts[i].h = h;

        if(steps[i].generateGgxFiltered)
        {
            auto& srcImg = steps[i].srcStep == -1 ? inImg : stepOuts[steps[i].srcStep].dsImg;
            stepOuts[i].ggxTex = createGgxFilteredTex(srcImg, steps[i].rough2, steps[i].numSamples);
            tg::Img3f ggxImg(w, h);
            glReadPixels(0, 0, w, h, GL_RGB, GL_FLOAT, ggxImg.data());
            tl::toStringBuffer(s_buffer, "test_ggx_", i+1, ".hdr");
            ggxImg.save(s_buffer);
        }
    }

    return true;
}

int main(int argc, char* argv[])
{
    if(argc != 2)
        printf("wrong number of args\n");

    if(strcmp(argv[1], "filter_cubemap") == 0)
        return filterCubemap() ? 0 : 1;

    return 0;
}

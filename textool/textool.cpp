#include <stdio.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stbi.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>
#include <tl/basic.hpp>
#include <tl/basic_math.hpp>
#include <tl/fmt.hpp>
#include <tl/defer.hpp>
#include <tg/texture_utils.hpp>
#include <tg/shader_utils.hpp>

static char s_buffer[4*1024];

static void glErrorCallback(const char *name, void *funcptr, int len_args, ...)
{
    GLenum error_code;
    error_code = glad_glGetError();
    if (error_code != GL_NO_ERROR) {
        fprintf(stderr, "ERROR %s in %s\n", gluErrorString(error_code), name);
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
    glBindTexture(GL_TEXTURE_CUBE_MAP, inTex);
    tg::simpleInitCubemapTexture();
    int sidePixels = inImg.width() / 4;
    assert(sidePixels == inImg.height() / 3);
    tg::uploadCubemapTexture(0, inImg.width(), inImg.height(), GL_RGB, GL_RGB, GL_FLOAT, (u8*)inImg.data());

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
            return false;
        }
    }
    defer(glDeleteProgram(ggxFilterProg));
    const tg::GgxFilterUnifLocs ggxFilterUnifLocs = tg::getFilterCubamap_ggx_unifLocs(ggxFilterProg);

    u32 vao, vbo, numVerts;
    tg::createFilterCubemapMeshGpu(vao, vbo, numVerts);

    u32 framebuffer;
    u32 rbo;
    glGenFramebuffers(1, &framebuffer);
    defer(glDeleteFramebuffers(1, &framebuffer));
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glGenRenderbuffers(1, &rbo);
    defer(glDeleteRenderbuffers(1, &rbo));
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB16F, 4*sidePixels, 3*sidePixels);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glClearColor(0,0,0,1);

    // GGX filtering
    glBindVertexArray(vao);
    glUseProgram(ggxFilterProg);
    glUniform1i(ggxFilterUnifLocs.cubemap, 0);

    const int w = 4 * sidePixels;
    const int h = 3 * sidePixels;
    glUniform1f(ggxFilterUnifLocs.roughness2, 0.01f);
    glUniform1ui(ggxFilterUnifLocs.numSamples, 10000u);
    glViewport(0, 0, w, h);
    glScissor(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    tg::Img3f outImg(w, h);
    glReadPixels(0, 0, w, h, GL_RGB, GL_FLOAT, outImg.data());
    outImg.save("out.hdr");

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

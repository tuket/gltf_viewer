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

    const u32 vertShader = tg::createFilterCubemapVertShader();
    const u32 fragShader = tg::createFilterCubemap_ggx_fragShader();
    const u32 prog = glCreateProgram();
    glAttachShader(prog, vertShader);
    glAttachShader(prog, fragShader);
    glLinkProgram(prog);
    if(const char* errMsg = tg::getShaderLinkErrors(prog, s_buffer)) {
        tl::println("Error linking shader:\n", errMsg);
        return false;
    }
    const tg::GgxFilterUnifLocs unifLocs = tg::getFilterCubamap_ggx_unifLocs(prog);
    u32 vao, vbo, numVerts;
    tg::createFilterCubemapMeshGpu(vao, vbo, numVerts);
    auto error = tg::filterCubemap_GGX(
        "test.hdr",
        "autumn_ggx_", ".hdr",
        prog, vao, unifLocs);
    if(error == tg::FilterCubemapError::CANT_OPEN_INPUT_FILE) {
        tl::println("Error: could not open input file");
    }
    else if(error == tg::FilterCubemapError::CANT_OPEN_OUTPUT_FILE){
        tl::println("Error: could not write to output files");
    }
    return error != tg::FilterCubemapError::NONE;
}

int main(int argc, char* argv[])
{
    if(argc != 2)
        printf("wrong number of args\n");

    if(strcmp(argv[1], "filter_cubemap"))
        return filterCubemap() ? 0 : 1;

    return 0;
}

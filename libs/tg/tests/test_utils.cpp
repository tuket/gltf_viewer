#include "test_utils.hpp"

#include <stdio.h>
#include <glad/glad.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>
#include <tl/defer.hpp>
#include <tl/fmt.hpp>
#include <tg/shader_utils.hpp>

constexpr float PI = glm::pi<float>();

char g_buffer[4*1024];

static void glErrorCallback(const char *name, void *funcptr, int len_args, ...) {
    GLenum error_code;
    error_code = glad_glGetError();
    if (error_code != GL_NO_ERROR) {
        fprintf(stderr, "ERROR %s in %s\n", gluErrorString(error_code), name);
        assert(false);
    }
}

GLFWwindow* simpleInitGlfwGL()
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

void OrbitCameraInfo::applyMouseDrag(glm::vec2 deltaPixels, glm::vec2 screenSize)
{
    constexpr float speed = PI;
    heading += speed * deltaPixels.x / screenSize.x;
    while(heading < 0)
        heading += 2*PI;
    while(heading > 2*PI)
        heading -= 2*PI;
    pitch += speed * deltaPixels.y / screenSize.y;
    pitch = glm::clamp(pitch, -0.45f*PI, +0.45f*PI);
}

void OrbitCameraInfo::applyMouseWheel(float dy)
{
    constexpr float speed = 1.04f;
    distance *= pow(speed, (float)dy);
    distance = glm::max(distance, 0.01f);
}

static bool mousePressed;
static double prevMouseX, prevMouseY;
static OrbitCameraInfo* s_orbitCamInfo = nullptr;
void addSimpleOrbitCameraBaviour(GLFWwindow* window, OrbitCameraInfo& orbitCamInfo)
{
    s_orbitCamInfo = &orbitCamInfo;
    mousePressed = false;

    auto onMouseClick = +[](GLFWwindow* window, int button, int action, int mods)
    {
        if(button == GLFW_MOUSE_BUTTON_1)
            mousePressed = action == GLFW_PRESS;
    };

    auto onMouseMove = [](GLFWwindow* window, double x, double y)
    {
        assert(s_orbitCamInfo);
        auto& orbitCam = *s_orbitCamInfo;
        if(mousePressed) {
            const float dx = (float)x - prevMouseX;
            const float dy = (float)y - prevMouseY;
            int windowW, windowH;
            glfwGetWindowSize(window, &windowW, &windowH);
            orbitCam.applyMouseDrag({dx, dy}, {windowW, windowH});
        }
        prevMouseX = (float)x;
        prevMouseY = (float)y;
    };

    auto onMouseWheel = [](GLFWwindow* window, double dx, double dy)
    {
        assert(s_orbitCamInfo);
        auto& orbitCam = *s_orbitCamInfo;
        orbitCam.applyMouseWheel(dy);
    };

    glfwSetMouseButtonCallback(window, onMouseClick);
    glfwSetCursorPosCallback(window, onMouseMove);
    glfwSetScrollCallback(window, onMouseWheel);
}

const char g_cubemapVertShadSrc[] =
R"GLSL(
#version 330 core

in layout(location = 0) vec3 a_pos;

out vec3 v_modelPos;

uniform mat4 u_modelViewProjMat;


void main()
{
    v_modelPos = a_pos;
    gl_Position = u_modelViewProjMat * vec4(a_pos, 1);
}
)GLSL";

const char g_cubemapFragShadSrc[] =
R"GLSL(
#version 330 core

layout(location = 0) out vec4 o_color;

in vec3 v_modelPos;

uniform samplerCube u_cubemap;

void main()
{
    o_color = texture(u_cubemap, normalize(v_modelPos));
}
)GLSL";

void createSimpleCubemapShader(u32& prog, SimpleCubemapShaderUnifLocs& unifLocs)
{
    const u32 vertShad = glCreateShader(GL_VERTEX_SHADER);
    defer(glDeleteShader(vertShad));
    static const char* vertShadSrcPtr = g_cubemapVertShadSrc;
    glShaderSource(vertShad, 1, &vertShadSrcPtr, nullptr);
    glCompileShader(vertShad);
    if(const char* errMsg = tg::getShaderCompileErrors(vertShad, g_buffer)) {
        tl::println("Error compiling vertex shader: ", errMsg);
        assert(false);
    }

    const u32 fragShad = glCreateShader(GL_FRAGMENT_SHADER);
    defer(glDeleteShader(fragShad));
    static const char* fragShadSrcPtr = g_cubemapVertShadSrc;
    glShaderSource(fragShad, 1, &fragShadSrcPtr, nullptr);
    glCompileShader(fragShad);

    prog = glCreateProgram();
    glAttachShader(prog, vertShad);
    glAttachShader(prog, fragShad);
    glLinkProgram(prog);
    if(const char* errMsg = tg::getShaderLinkErrors(prog, g_buffer)) {
        tl::println("Error compiling fragment shader: ", errMsg);
        assert(false);
    }

    glUseProgram(prog);
    unifLocs.modelViewProj = glGetUniformLocation(prog, "u_modelViewProjMat");
    assert(unifLocs.modelViewProj != -1);
    unifLocs.cubemap = glGetUniformLocation(prog, "u_cubemap");
    assert(unifLocs.cubemap != -1);
}

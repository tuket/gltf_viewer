#include "test_utils.hpp"

#include <stdio.h>
#include <glad/glad.h>
#include <GL/glu.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>

constexpr float PI = glm::pi<float>();

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
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "test", nullptr, nullptr);
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


static bool mousePressed;
static double prevMouseX, prevMouseY;
static OrbitCameraInfo* s_orbitCamInfo = nullptr;
void addOrbitCameraBaviour(GLFWwindow* window, OrbitCameraInfo& orbitCamInfo)
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
            constexpr float speed = PI;
            int windowW, windowH;
            glfwGetWindowSize(window, &windowW, &windowH);
            orbitCam.heading += speed * dx / windowW;
            while(orbitCam.heading < 0)
                orbitCam.heading += 2*PI;
            while(orbitCam.heading > 2*PI)
                orbitCam.heading -= 2*PI;
            orbitCam.pitch += speed * dy / windowH;
            orbitCam.pitch = glm::clamp(orbitCam.pitch, -0.45f*PI, +0.45f*PI);
        }
        prevMouseX = (float)x;
        prevMouseY = (float)y;
    };

    auto onMouseWheel = [](GLFWwindow* window, double dx, double dy)
    {
        assert(s_orbitCamInfo);
        auto& orbitCam = *s_orbitCamInfo;
        constexpr float speed = 1.04f;
        orbitCam.distance *= pow(speed, (float)dy);
        orbitCam.distance = glm::max(orbitCam.distance, 0.01f);
    };

    glfwSetMouseButtonCallback(window, onMouseClick);
    glfwSetCursorPosCallback(window, onMouseMove);
    glfwSetScrollCallback(window, onMouseWheel);
}


#include <stdio.h>
#define GLAD_DEBUG
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <tl/fmt.hpp>
#include <glm/vec2.hpp>
#include <GL/glu.h>
#include "scene.hpp"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "utils.hpp"
#include "shaders.hpp"

GLFWwindow* window;


void glErrorCallback(const char *name, void *funcptr, int len_args, ...) {
    GLenum error_code;
    error_code = glad_glGetError();
    if (error_code != GL_NO_ERROR) {
        fprintf(stderr, "ERROR %s in %s\n", gluErrorString(error_code), name);
        assert(false);
    }
}

int main(int argc, char* argv[])
{
    glfwSetErrorCallback(+[](int error, const char* description) {
        fprintf(stderr, "Glfw Error %d: %s\n", error, description);
    });
    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    window = glfwCreateWindow(1280, 720, "test", nullptr, nullptr);
    if (window == nullptr)
        return 1;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // Enable vsync

    if (gladLoadGL() == 0) {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return 1;
    }
    glad_set_post_callback(glErrorCallback);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();
    io.Fonts->AddFontDefault();
    const int fontSize = 18;
    /*{
        ImFontConfig font_cfg = ImFontConfig();
        font_cfg.OversampleH = font_cfg.OversampleV = 4;
        font_cfg.PixelSnapH = true;
        font_cfg.SizePixels = fontSize;
        const char* file = "ProggyClean.ttf";
        tl::toStringBuffer(font_cfg.Name, file, ", ", fontSize, "px");
        font_cfg.EllipsisChar = (ImWchar)0x0085;
        auto font = io.Fonts->AddFontFromFileTTF(file, fontSize, &font_cfg);
        //fonts::roboto->DisplayOffset.y = -1.0f;
    }*/
    /*{
        ImFontConfig font_cfg = ImFontConfig();
        font_cfg.OversampleH = font_cfg.OversampleV = 1;
        font_cfg.PixelSnapH = true;
        const int size = 18;
        font_cfg.SizePixels = size;
        const char* file = "RobotoMono-Regular.ttf";
        tl::toStringBuffer(font_cfg.Name, file, ", ", fontSize, "px");
        font_cfg.EllipsisChar = (ImWchar)0x0085;
        fonts::roboto = io.Fonts->AddFontFromFileTTF(file, fontSize, &font_cfg);
        fonts::roboto->DisplayOffset.y = -1.0f;
    }*/
    /*{
        ImFontConfig font_cfg = ImFontConfig();
        font_cfg.OversampleH = font_cfg.OversampleV = 1;
        font_cfg.PixelSnapH = true;
        font_cfg.SizePixels = fontSize;
        const char* file = "RobotoMono-Bold.ttf";
        tl::toStringBuffer(font_cfg.Name, file, ", ", fontSize, "px");
        font_cfg.EllipsisChar = (ImWchar)0x0085;
        fonts::robotoBold = io.Fonts->AddFontFromFileTTF(file, fontSize, &font_cfg);
        fonts::robotoBold->DisplayOffset.y = -1.0f;
    }*/

    gpu::buildShaders();

    glfwSetDropCallback(window, onFileDroped);
    if(argc > 1) {
        loadGltf(argv[1]);
    }

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // draw scene
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glScissor(0, 0, w, h);
        drawScene();

        // draw gui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        drawGui();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwWaitEventsTimeout(0.01);
    }
}

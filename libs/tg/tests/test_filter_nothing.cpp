#include "test_utils.hpp"
#include <stdio.h>
#include <assert.h>
#include <glad/glad.h>
#include <stb/stbi.h>
#include <GLFW/glfw3.h>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <tl/containers/fvector.hpp>
#include <tl/fmt.hpp>
#include <tl/defer.hpp>
#include <tl/basic.hpp>
#include <tg/texture_utils.hpp>
#include <tg/img.hpp>
#include <tg/mesh_utils.hpp>
#include <tg/shader_utils.hpp>
#include <tg/cameras.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

static char s_buffer[4*1024];


static void drawGui()
{
    ImGui::Begin("test", 0, 0);

    ImGui::End();
}


bool test_filterNothing()
{
    GLFWwindow* window = simpleInitGlfwGL();

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int /*scanCode*/, int action, int /*mods*/)
    {
        if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
    });
    glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int /*mods*/)
    {
        if (ImGui::GetIO().WantCaptureMouse) {
            //s_mousePressed = false;
            return;
        }
        if(button == GLFW_MOUSE_BUTTON_1)
        {
            //s_mousePressed = action == GLFW_PRESS;
            //s_draggingSplitter = s_mousePressed && hoveringSplitter(window);
        }
    });
    glfwSetCursorPosCallback(window, [](GLFWwindow* window, double x, double y)
    {

    });
    glfwSetScrollCallback(window, [](GLFWwindow* /*window*/, double /*dx*/, double dy)
    {
        if (ImGui::GetIO().WantCaptureMouse)
            return;
    });

    {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsClassic();

        // Setup Platform/Renderer bindings
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init();
    }

    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        int screenW, screenH;
        glfwGetFramebufferSize(window, &screenW, &screenH);
        glViewport(0, 0, screenW, screenH);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        drawGui();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwWaitEventsTimeout(0.01);
    }

    return true;
}

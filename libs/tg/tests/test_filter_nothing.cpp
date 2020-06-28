#include "test_utils.hpp"
#include <stdio.h>
#include <assert.h>
#include <glad/glad.h>
#include <stb/stbi.h>
#include <GLFW/glfw3.h>
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

using glm::vec2;

static char s_buffer[4*1024];

static float s_scale = 1;
static vec2 s_offset = {0, 0};
static bool s_smooth = false;
static u32 originalTex, tempTex1, tempTex2;

static void drawGui()
{
    ImGui::Begin("test", 0, 0);
    ImGui::Checkbox("smooth", &s_smooth);
    ImGui::SliderFloat("scale", &s_scale, 0, 2);
    ImGui::End();
}

void computeDowscaleTexLog2(u32& outTex, i32& outTexW, i32& outTexH,
    u32 originalTex, i32 originalTexW, i32 originalTexH, float scale)
{
    outTexW = originalTexW;
    outTexH = originalTexH;
    outTex = originalTex;
    if(scale < 0.5f)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, s_tempFramebuffer);
        glBindVertexArray(s_fsVao);
        glUseProgram(s_filterNothingProg);
        while(scale < 0.5f)
        {
            glBindTexture(GL_TEXTURE_2D, outTex);
            glUniform2f(s_filterNothingUnifs.texRegion,
                float(outTexW) / originalTexW, float(outTexH) / originalTexH);
            outTexW /= 2;
            outTexH /= 2;
            outTex = outTex == tempTex1 ? tempTex2 : tempTex1;
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTex, 0);
            glViewport(0, 0, outTexW, outTexH);
            glScissor(0, 0, outTexW, outTexH);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            scale *= 2;
        }
    }
}

static const char* s_vertShadSrc =
R"GLSL(
#version 330 core

layout (location = 0) in vec2 a_pos;
out vec2 v_tc;

uniform vec2 u_offset;
uniform vec2 u_scale;

void main()
{
    v_tc = 0.5 * (a_pos + 1.0);
    v_tc.y = - v_tc.y;
    gl_Position = vec4(u_offset + u_scale * a_pos, 0.0, 1.0);
}
)GLSL";

static const char* s_fragShadSrc =
R"GLSL(
#version 330 core
layout (location = 0) out vec4 o_color;
in vec2 v_tc;

uniform sampler2D u_tex;

void main()
{
    o_color = pow(texture(u_tex, v_tc), vec4(1.0/2.2));
}
)GLSL";

bool test_filterNothing()
{
    GLFWwindow* window = simpleInitGlfwGL();

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int /*scanCode*/, int action, int /*mods*/)
    {
        if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
    });
    /*glfwSetMouseButtonCallback(window, [](GLFWwindow* window, ){

    })*/;
    glfwSetScrollCallback(window, [](GLFWwindow* window, double dx, double dy){
        s_scale *= pow(1.1, dy);
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

    auto setCommonTexParams = []() {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    };

    glActiveTexture(GL_TEXTURE0);
    tg::Img3f img = tg::Img3f::load("autumn_cube.hdr");
    glGenTextures(1, &originalTex);
    defer(glDeleteTextures(1, &originalTex));
    glBindTexture(GL_TEXTURE_2D, originalTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img.width(), img.height(), 0, GL_RGB, GL_FLOAT, img.data());
    setCommonTexParams();

    glGenTextures(1, &tempTex1);
    defer(glDeleteTextures(1, &tempTex1));
    glBindTexture(GL_TEXTURE_2D, tempTex1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img.width(), img.height(), 0, GL_RGB, GL_FLOAT, nullptr);
    setCommonTexParams();

    glGenTextures(1, &tempTex2);
    defer(glDeleteTextures(1, &tempTex2));
    glBindTexture(GL_TEXTURE_2D, tempTex2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img.width(), img.height(), 0, GL_RGB, GL_FLOAT, nullptr);
    setCommonTexParams();

    u32 vao, vbo, numVerts;
    tg::createScreenQuadMesh2D(vao, vbo, numVerts);
    defer(glDeleteVertexArrays(1, &vao));
    defer(glDeleteBuffers(1, &vbo));

    const u32 vertShad = glCreateShader(GL_VERTEX_SHADER);
    defer(glDeleteShader(vertShad));
    glShaderSource(vertShad, 1, &s_vertShadSrc, nullptr);
    glCompileShader(vertShad);
    if(const char* errMsg = tg::getShaderCompileErrors(vertShad, s_buffer)) {
        tl::println("Error compiling vertex shader: ", errMsg);
        return 1;
    }

    const u32 fragShad = glCreateShader(GL_FRAGMENT_SHADER);
    defer(glDeleteShader(fragShad));
    glShaderSource(fragShad, 1, &s_fragShadSrc, nullptr);
    glCompileShader(fragShad);
    if(const char* errMsg = tg::getShaderCompileErrors(fragShad, s_buffer)) {
        tl::println("Error compiling fragment shader: ", errMsg);
        return 1;
    }

    const u32 prog = glCreateProgram();
    defer(glDeleteProgram(prog));
    glAttachShader(prog, vertShad);
    glAttachShader(prog, fragShad);
    glad_glLinkProgram(prog);
    if(const char* errMsg = tg::getShaderLinkErrors(prog, s_buffer)) {
        tl::println("Error linking program: ", errMsg);
        return 1;
    }
    const u32 unif_tex = glGetUniformLocation(prog, "u_tex");
    const u32 unif_offset = glGetUniformLocation(prog, "u_offset");
    const u32 unif_scale = glGetUniformLocation(prog, "u_scale");
    glUseProgram(prog);
    glUniform1i(unif_tex, 0);

    glClearColor(0.2, 0.2, 0.2, 1);
    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        int screenW, screenH;
        glfwGetFramebufferSize(window, &screenW, &screenH);
        glViewport(0, 0, screenW, screenH);

        glClear(GL_COLOR_BUFFER_BIT);
        glUniform2fv(unif_offset, 1, &s_offset[0]);
        const vec2 scale = s_scale * vec2(img.width(), img.height()) / vec2(screenH, screenH);
        glUniform2fv(unif_scale, 1, &scale[0]);
        glDrawArrays(GL_TRIANGLES, 0, 6);

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

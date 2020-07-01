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
static u32 s_tempFramebuffer;
static u32 s_originalTex, s_tempTex1, s_tempTex2;
static u32 s_lastPassProg, s_filterNothingProg;
static struct {u32 tex, texRegionMax, offset, scale;} s_lastPassLocs;
static tg::FilterNothingUnifLocs s_filterNothingLocs;
static u32 s_fsVao, s_fsVbo;

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
    if(scale < 1)
    {
        float outScale = scale;
        glBindFramebuffer(GL_FRAMEBUFFER, s_tempFramebuffer);
        glBindVertexArray(s_fsVao);
        glUseProgram(s_filterNothingProg);
        while(outScale < 0.5f)
        {
            glBindTexture(GL_TEXTURE_2D, outTex);
            glUniform2f(s_filterNothingLocs.texRegionMin, 0, 0);
            glUniform2f(s_filterNothingLocs.texRegionMax,
                float(outTexW) / originalTexW, float(outTexH) / originalTexH);
            outTexW /= 2;
            outTexH /= 2;
            outTex = (outTex == s_tempTex1) ? s_tempTex2 : s_tempTex1;
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTex, 0);
            glViewport(0, 0, outTexW, outTexH);
            glScissor(0, 0, outTexW, outTexH);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            outScale *= 2;
        }
        if(outScale != 1)
        {
            glBindTexture(GL_TEXTURE_2D, outTex);
            glUniform2f(s_filterNothingLocs.texRegionMin, 0, 0);
            glUniform2f(s_filterNothingLocs.texRegionMax,
                float(outTexW) / originalTexW, float(outTexH) / originalTexH);
            outTexW = float(outTexW) * outScale;
            outTexH = float(outTexH) * outScale;
            outTex = (outTex == s_tempTex1) ? s_tempTex2 : s_tempTex1;
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTex, 0);
            glViewport(0, 0, outTexW, outTexH);
            glScissor(0, 0, outTexW, outTexH);
            glDrawArrays(GL_TRIANGLES, 0, 6);
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
uniform vec2 u_texRegionMax;

void main()
{
    v_tc = 0.5 * (a_pos + 1.0);
    v_tc.y = 1 - v_tc.y;
    v_tc *= u_texRegionMax;
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
    vec4 color = texture(u_tex, v_tc);
    o_color = vec4(pow(color.rgb, vec3(1.0/2.2)), color.a);
}
)GLSL";

static bool s_mousePessed = false;
static vec2 s_prevMousePos;

bool test_downscale()
{
    GLFWwindow* window = simpleInitGlfwGL();

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int /*scanCode*/, int action, int /*mods*/)
    {
        if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
    });
    glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mods){
        if(button == GLFW_MOUSE_BUTTON_1) {
            s_mousePessed = action == GLFW_PRESS;
        }
    });
    glfwSetCursorPosCallback(window, [](GLFWwindow* window, double x, double y) {
        const vec2 mousePos(x, y);
        if(s_mousePessed) {
            int screenW, screenH;
            glfwGetFramebufferSize(window, &screenW, &screenH);
            vec2 move = mousePos - s_prevMousePos;
            move = 2.f * move / vec2(screenW, -screenH);
            s_offset += move;
            s_offset = glm::clamp(s_offset, {-1,-1}, {+1, +1});
        }
        s_prevMousePos = mousePos;
    });
    glfwSetScrollCallback(window, [](GLFWwindow* window, double dx, double dy){
        s_scale *= pow(1.03, dy);
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

    u32 numVerts;
    tg::createScreenQuadMesh2D(s_fsVao, s_fsVbo, numVerts);
    defer(glDeleteVertexArrays(1, &s_fsVao));
    defer(glDeleteBuffers(1, &s_fsVbo));

    glGenFramebuffers(1, &s_tempFramebuffer);

    auto setCommonTexParams = []() {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    };

    glActiveTexture(GL_TEXTURE0);
    tg::Img3f img = tg::Img3f::load("autumn_cube.hdr");
    glGenTextures(1, &s_originalTex);
    defer(glDeleteTextures(1, &s_originalTex));
    glBindTexture(GL_TEXTURE_2D, s_originalTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img.width(), img.height(), 0, GL_RGB, GL_FLOAT, img.data());
    setCommonTexParams();

    glGenTextures(1, &s_tempTex1);
    defer(glDeleteTextures(1, &s_tempTex1));
    glBindTexture(GL_TEXTURE_2D, s_tempTex1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img.width(), img.height(), 0, GL_RGB, GL_FLOAT, nullptr);
    setCommonTexParams();

    glGenTextures(1, &s_tempTex2);
    defer(glDeleteTextures(1, &s_tempTex2));
    glBindTexture(GL_TEXTURE_2D, s_tempTex2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img.width(), img.height(), 0, GL_RGB, GL_FLOAT, nullptr);
    setCommonTexParams();

    { // create last pass shader program
        const u32 vertShad = glCreateShader(GL_VERTEX_SHADER);
        defer(glDeleteShader(vertShad));
        glShaderSource(vertShad, 1, &s_vertShadSrc, nullptr);
        glCompileShader(vertShad);
        if(const char* errMsg = tg::getShaderCompileErrors(vertShad, s_buffer)) {
            tl::println("Error compiling vertex shader: ", errMsg);
            assert(false);
        }

        const u32 fragShad = glCreateShader(GL_FRAGMENT_SHADER);
        defer(glDeleteShader(fragShad));
        glShaderSource(fragShad, 1, &s_fragShadSrc, nullptr);
        glCompileShader(fragShad);
        if(const char* errMsg = tg::getShaderCompileErrors(fragShad, s_buffer)) {
            tl::println("Error compiling fragment shader: ", errMsg);
            assert(false);
        }

        s_lastPassProg = glCreateProgram();
        glAttachShader(s_lastPassProg, vertShad);
        glAttachShader(s_lastPassProg, fragShad);
        glad_glLinkProgram(s_lastPassProg);
        if(const char* errMsg = tg::getShaderLinkErrors(s_lastPassProg, s_buffer)) {
            tl::println("Error linking program: ", errMsg);
            assert(false);
        }
    }
    defer(glDeleteProgram(s_lastPassProg));
    s_lastPassLocs.tex = glGetUniformLocation(s_lastPassProg, "u_tex");
    s_lastPassLocs.texRegionMax = glGetUniformLocation(s_lastPassProg, "u_texRegionMax");
    s_lastPassLocs.offset = glGetUniformLocation(s_lastPassProg, "u_offset");
    s_lastPassLocs.scale = glGetUniformLocation(s_lastPassProg, "u_scale");
    glUseProgram(s_lastPassProg);
    glUniform1i(s_lastPassLocs.tex, 0);

    { // create filter nothing shader program
        s_filterNothingProg = glCreateProgram();
        const u32 vertShad = tg::createFilterVertShader();
        defer(glDeleteShader(vertShad));
        const u32 fragShad = tg::createFilterNothingFragShader();
        defer(glDeleteShader(fragShad));
        glAttachShader(s_filterNothingProg, vertShad);
        glAttachShader(s_filterNothingProg, fragShad);
        glLinkProgram(s_filterNothingProg);
        if(const char* errMsg = tg::getShaderLinkErrors(s_filterNothingProg, s_buffer)) {
            tl::println("Error linking program: ", errMsg);
            return 1;
        }
    }
    defer(glDeleteProgram(s_filterNothingProg));
    tg::getFilterNothingUnifLocs(s_filterNothingLocs, s_filterNothingProg);

    glClearColor(0.2, 0.2, 0.2, 1);
    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        int screenW, screenH;
        glfwGetFramebufferSize(window, &screenW, &screenH);

        glClear(GL_COLOR_BUFFER_BIT);
        if(s_scale > 0)
        {
            u32 dsTex;
            i32 dsW, dsH;
            if(s_smooth)
                computeDowscaleTexLog2(dsTex, dsW, dsH, s_originalTex, img.width(), img.height(), s_scale);
            else {
                dsTex = s_originalTex;
                dsW = img.width();
                dsH = img.height();
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, screenW, screenH);
            glBindTexture(GL_TEXTURE_2D, dsTex);
            glUseProgram(s_lastPassProg);
            glUniform2f(s_lastPassLocs.texRegionMax,
                (float(dsW)-0.5f) / img.width(), (float(dsH)-0.5f) / img.height());
            glUniform2fv(s_lastPassLocs.offset, 1, &s_offset[0]);
            const vec2 scale = s_scale * vec2(img.width(), img.height()) / vec2(screenW, screenH);
            glUniform2fv(s_lastPassLocs.scale, 1, &scale[0]);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

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

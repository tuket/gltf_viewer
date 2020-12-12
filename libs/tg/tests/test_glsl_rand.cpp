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
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

using glm::vec3;

static char s_buffer[4*1024];
u32 s_numSamples = 16;
i32 s_numSamplesLoc;
u32 s_prog;

static const char k_vertShadSrc[] =
R"GLSL(
layout (location = 0) in vec2 a_pos;

out vec2 v_tc;

void main()
{
    v_tc = 0.5 * a_pos + 0.5;
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)GLSL";

static const char k_fragShadSrc[] =
R"GLSL(
layout (location = 0) out vec4 o_color;

uniform uint u_numSamples;
uniform float u_pointSize = 0.003;

in vec2 v_tc;


// Construct a float with half-open range [0:1] using low 23 bits.
// All zeroes yields 0.0, all ones yields the next smallest representable value below 1.0.
// https://stackoverflow.com/a/17479300/1754322
float makeFloat01( uint m ) {
    const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint ieeeOne      = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    float  f = uintBitsToFloat( m );       // Range [1:2]
    return f - 1.0;                        // Range [0:1]
}

void main()
{
    float c = 0;
    for(uint iSample = 0u; iSample < u_numSamples; iSample++)
    {
        uvec3 seed = pcg_uvec3_uvec3(uvec3(43, 535, iSample));
        vec2 seed2 = vec2(makeFloat01(seed.x), makeFloat01(seed.y));
        if(length(seed2 - v_tc) < u_pointSize)
            c = 1;
    }
    o_color = vec4(vec3(c), 0);
}
)GLSL";


static void drawGui()
{
    ImGui::Begin("giterator", 0, 0);
    {
        int numSamples = s_numSamples;
        constexpr int maxSamples = 4*1024;
        ImGui::SliderInt("Samples", &numSamples, 1, maxSamples);
        s_numSamples = tl::clamp(numSamples, 1, maxSamples);
    }
    ImGui::End();
}

bool test_glslRand()
{
    GLFWwindow* window = simpleInitGlfwGL();

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int /*scanCode*/, int action, int /*mods*/)
    {
        if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
    });
    glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int /*mods*/)
    {
    });
    glfwSetCursorPosCallback(window, [](GLFWwindow* window, double x, double y)
    {
    });
    glfwSetScrollCallback(window, [](GLFWwindow* /*window*/, double /*dx*/, double dy)
    {
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

    // init object mesh
    u32 vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    u32 vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(tg::k_screenQuad2DVerts), tg::k_screenQuad2DVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    s_prog = glCreateProgram();
    {
        const char* vertSrcs[] = { tg::srcs::header, k_vertShadSrc };
        const char* fragSrcs[] = {
            tg::srcs::header,
            tg::srcs::hammersley,
            tg::srcs::pcg_uvec3_uvec3,
            k_fragShadSrc};
        constexpr int numVertSrcs = tl::size(vertSrcs);
        constexpr int numFragSrcs = tl::size(fragSrcs);
        int srcsSizes[tl::max(numVertSrcs, numFragSrcs)];

        u32 vertShad = glCreateShader(GL_VERTEX_SHADER);
        defer(glDeleteShader(vertShad));
        for(int i = 0; i < numVertSrcs; i++)
            srcsSizes[i] = strlen(vertSrcs[i]);
        glShaderSource(vertShad, numVertSrcs, vertSrcs, srcsSizes);
        glCompileShader(vertShad);
        if(const char* errMsg = tg::getShaderCompileErrors(vertShad, s_buffer)) {
            tl::println("Error compiling vertex shader:\n", errMsg);
            return 1;
        }

        u32 fragShad = glCreateShader(GL_FRAGMENT_SHADER);
        defer(glDeleteShader(fragShad));
        for(int i = 0; i < numFragSrcs; i++)
            srcsSizes[i] = strlen(fragSrcs[i]);
        glShaderSource(fragShad, numFragSrcs, fragSrcs, srcsSizes);
        glCompileShader(fragShad);
        if(const char* errMsg = tg::getShaderCompileErrors(fragShad, s_buffer)) {
            tl::println("Error compiling frament shader:\n", errMsg);
            return 1;
        }

        glAttachShader(s_prog, vertShad);
        glAttachShader(s_prog, fragShad);
        glLinkProgram(s_prog);
        if(const char* errMsg = tg::getShaderLinkErrors(s_prog, s_buffer)) {
            tl::println("Error linking program:\n", errMsg);
            return 1;
        }

        s_numSamplesLoc = glGetUniformLocation(s_prog, "u_numSamples");
    }
    defer(glDeleteProgram(s_prog));

    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        int screenW, screenH;
        glfwGetFramebufferSize(window, &screenW, &screenH);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        drawGui();

        glViewport(0, 0, screenW, screenH);
        glScissor(0, 0, screenW, screenH);

        glUseProgram(s_prog);
        if(s_numSamplesLoc != -1)
            glUniform1ui(s_numSamplesLoc, s_numSamples);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwWaitEventsTimeout(0.01);
    }

    return true;
}

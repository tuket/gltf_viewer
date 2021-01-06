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

using glm::vec3;

static GLFWcursor* s_splitterCursor = nullptr;
static char s_buffer[4*1024];
static bool s_mousePressed = false;
static glm::vec2 s_prevMouse;
static OrbitCameraInfo s_orbitCam;
static u32 s_fbo;
static u32 s_rt[2], s_depthRbo; // render textures
static struct { u32 envmap, convolution; } s_textures;
static u32 s_screenQuadVbo, s_screenQuadVao;
static u32 s_envCubeVao, s_envCubeVbo;
static u32 s_objVao, s_objVbo, s_objEbo, s_objNumInds;
static u32 s_splatProg, s_envProg, s_iblProg, s_rtProg;
static struct { i32 tex; } s_splatUnifLocs;
static struct { i32 modelViewProj, cubemap, gammaExp; } s_envShadUnifLocs;
struct CommonUnifLocs { i32 camPos, model, modelViewProj, albedo, rough2, metallic, F0, convolutedEnv, lut; };
static struct : public CommonUnifLocs {  } s_iblUnifLocs;
static struct : public CommonUnifLocs {
    i32 numSamples, numFramesWithoutChanging,
        fresnelTerm, geomTerm, ndfTerm, test;
} s_rtUnifLocs;
static float s_splitterPercent = 0.5;
static bool s_draggingSplitter = false;
static float s_rough = 0.1f;
static u32 s_numSamplesPerFrame = 64;
static u32 s_numFramesWithoutChanging = 0; // the number of frames we have been drawing the exact same thing, we use this to compute the blenfing factor inorder to apply temporal antialiasing
static glm::vec3 s_albedo = {0.8f, 0.8f, 0.8f};
static float s_metallic = 0.99f;
static float s_enableFresnelTerm = 1;
static float s_enableGeomTerm = 1;
static float s_enableNdfTerm = 1;
static float s_testUnif = 0;

static const char k_splatVertShadSrc[] =
R"GLSL(
layout (location = 0) in vec2 a_pos;

out vec2 v_tc;

void main()
{
    v_tc = 0.5 + 0.5 * a_pos ;
    gl_Position = vec4(a_pos, 0, 1);
}
)GLSL";

static const char k_splatFragShadSrc[] =
R"GLSL(
layout (location = 0) out vec4 o_color;

in vec2 v_tc;

uniform sampler2D u_tex;

void main()
{
    vec4 color = texture(u_tex, v_tc);
    o_color = vec4(pow(color.rgb, vec3(1/2.2)), color.a);
}
)GLSL";

static const char k_vertShadSrc[] =
R"GLSL(
layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec3 a_normal;

uniform mat4 u_model;
uniform mat4 u_modelViewProj;

out vec3 v_pos;
out vec3 v_normal;

void main()
{
    vec4 worldPos4 = u_model * vec4(a_pos, 1.0);
    v_pos = worldPos4.xyz / worldPos4.w;
    v_normal = (u_model * vec4(a_normal, 0.0)).xyz;
    gl_Position = u_modelViewProj * vec4(a_pos, 1.0);
}
)GLSL";

static const char k_fragShadSrc[] =
R"GLSL(
layout (location = 0) out vec4 o_color;

uniform vec3 u_camPos;
uniform vec3 u_albedo;
uniform float u_rough2;
uniform float u_metallic;
uniform vec3 u_F0;
uniform samplerCube u_convolutedEnv;
uniform sampler2D u_lut;

in vec3 v_pos;
in vec3 v_normal;

void main()
{
    vec3 N = normalize(v_normal);
    vec3 V = normalize(u_camPos - v_pos);
    vec3 L = reflect(-V, N);
    vec3 env = texture(u_convolutedEnv, L).rgb;
        env = pow(env, vec3(1.0/2.2, 1.0/2.2, 1.0/2.2));
    o_color = vec4(env, 1.0);
    //o_color = vec4(normalize(N), 1.0);
}
)GLSL";

static const char k_rtFragShadSrc[] = // uses ray tracing to sample the environment
R"GLSL(
layout (location = 0) out vec4 o_uniform;
layout (location = 1) out vec4 o_importance;

uniform vec3 u_camPos;
uniform vec3 u_albedo;
uniform float u_rough2;
uniform float u_metallic;
uniform vec3 u_F0;
uniform samplerCube u_convolutedEnv;
uniform sampler2D u_lut;
uniform uint u_numSamples = 16u;
uniform uint u_numFramesWithoutChanging;

uniform float u_fresnelTerm;
uniform float u_geomTerm;
uniform float u_ndfTerm;
uniform float u_testUnif;

in vec3 v_pos;
in vec3 v_normal;

vec3 fresnelSchlick(float NoV, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - NoV, 5.0);
}

float ggx_G(float NoV, float rough2)
{
  float rough4 = rough2 * rough2;
  return 2.0 * (NoV) /
    (NoV + sqrt(rough4 + (1.0 - rough4) * NoV*NoV));
}

float ggx_G_smith(float NoV, float NoL, float rough2)
{
  return ggx_G(NoV, rough2) * ggx_G(NoL, rough2);
}

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

vec3 visualizeSamples()
{
    vec3 N = normalize(v_normal);
    float c = 0;
    for(uint iSample = 0u; iSample < u_numSamples; iSample++)
    {
        uvec3 seedUInt = pcg_uvec3_uvec3(uvec3(1234u, 16465u, iSample));
        vec2 seed2 = vec2(makeFloat01(seedUInt.x), makeFloat01(seedUInt.y));
        vec3 L = uniformSample(seed2, vec3(0, 1, 0));
        c += pow(max(0, dot(L, N)), 5000);
    }
    //c /= u_numSamples;
    return vec3(min(1, c));
}

void calcLighting()
{
    vec3 N = normalize(v_normal);
    vec3 V = normalize(u_camPos - v_pos);
    float NoV = max(0.0001, dot(N, V));

    vec3 F0 = mix(vec3(0.04), u_albedo, u_metallic);
    vec3 F = fresnelSchlick(NoV, F0);
        F = mix(vec3(1.0), F, u_fresnelTerm);
    vec3 k_spec = F;
    vec3 k_diff = (1 - k_spec) * (1 - u_metallic);


    // uniform sampling
    {
        vec3 color = vec3(0.0, 0.0, 0.0);
        for(uint iSample = 0u; iSample < u_numSamples; iSample++)
        {
            uint sampleId = iSample + u_numSamples * u_numFramesWithoutChanging;
            uvec3 seedUInt = pcg_uvec3_uvec3(uvec3(gl_FragCoord.x, gl_FragCoord.y, sampleId));
            vec2 seed2 = vec2(makeFloat01(seedUInt.x), makeFloat01(seedUInt.y));
            vec3 L = uniformSample(seed2, N);
            vec3 H = normalize(V + L);
            vec3 env = textureLod(u_convolutedEnv, L, 0.0).rgb;

            float NoL = max(0.0001, dot(N, L));
            float NoH = max(0.0001, dot(N, H));
            float NoH2 = NoH * NoH;
            float rough4 = u_rough2*u_rough2;
            float q = NoH2 * (rough4 - 1.0) + 1.0;
            float D = rough4 / (PI * q*q);
                D = mix(1.0, D, u_ndfTerm);
            float G = ggx_G_smith(NoV, NoL, u_rough2);
                G = mix(1.0, G, u_geomTerm);
            vec3 fr = F * G * D / (4 * NoV * NoL);
            vec3 diffuse = k_diff * u_albedo / PI;
            color += (diffuse + fr) * env * NoL;
        }
        color *= 2*PI / float(u_numSamples);
        o_uniform = vec4(color, 1);
    }
    // importance sampling
    {
        //uint validSamples = 0u;
        vec3 specular = vec3(0.0, 0.0, 0.0);
        for(uint iSample = 0u; iSample < u_numSamples; iSample++)
        {
            //vec2 seed2 = hammersleyVec2(iSample, u_numSamples);
            uint sampleId = iSample + u_numSamples * u_numFramesWithoutChanging;
            uvec3 seedUInt = pcg_uvec3_uvec3(uvec3(gl_FragCoord.x, gl_FragCoord.y, sampleId));
            seedUInt.xy += seedUInt.x;
            vec2 seed2 = vec2(makeFloat01(seedUInt.x), makeFloat01(seedUInt.y));
            vec3 H = importanceSampleGgxD(seed2, u_rough2, N);
            vec3 L = reflect(-V, H);
            if(dot(N, L) > 0) {
                vec3 env = textureLod(u_convolutedEnv, L, 0.0).rgb;
                float NoL = max(0.0001, dot(N, L));
                float NoH = max(0.0001, dot(N, H));
                float G = ggx_G_smith(NoV, NoL, u_rough2);
                    //G = mix(1.0, G, u_geomTerm);
                specular += env * F*G* dot(L, H) / (NoL * NoH);
                //validSamples++;
            }
            /*if((seed2.x >= 0 && seed2.x <= 1 && seed2.y >= 0 && seed2.y <= 1) == false) {
                o_importance = vec4(11111, 0, 0, 1);
                return;
            }*/
        }
        specular /= float(u_numSamples);
        o_importance = vec4(specular, 1);
    }    
}

void main()
{
    calcLighting();
    //o_color = vec4(visualizeSamples(), 1.0);
}
)GLSL";

static void drawGui()
{
    ImGui::Begin("giterator", 0, 0);
    {
        bool gottaRefresh = false;
        gottaRefresh |= ImGui::SliderFloat("Roughness", &s_rough, 0, 1.f, "%.5f", 1);
        int numSamples = s_numSamplesPerFrame;
        constexpr int maxSamples = 1024;
        ImGui::SliderInt("Samples per frame", &numSamples, 1, maxSamples);
        s_numSamplesPerFrame = tl::clamp(numSamples, 1, maxSamples);
        gottaRefresh |= ImGui::ColorEdit3("Albedo", &s_albedo[0]);
        gottaRefresh |= ImGui::SliderFloat("Metallic", &s_metallic, 0.f, 1.f);
        gottaRefresh |= ImGui::SliderFloat("Enable fresnel term", &s_enableFresnelTerm, 0, 1);
        gottaRefresh |= ImGui::SliderFloat("Enable geom term", &s_enableGeomTerm, 0, 1);
        gottaRefresh |= ImGui::SliderFloat("Enable NDF term", &s_enableNdfTerm, 0, 1);
        gottaRefresh |= ImGui::SliderFloat("Test", &s_testUnif, 0, 1);
        if(gottaRefresh)
            s_numFramesWithoutChanging = 0;
    }
    ImGui::End();
}

static bool hoveringSplitter(GLFWwindow* window)
{
    if (ImGui::GetIO().WantCaptureMouse)
        return false;
    int windowW, windowH;
    glfwGetWindowSize(window, &windowW, &windowH);
    const float splitterPixX = floorf(windowW * s_splitterPercent);
    return splitterPixX-2 <= s_prevMouse.x &&
           s_prevMouse.x <= splitterPixX+2;
}

bool test_iblPbr()
{
    GLFWwindow* window = simpleInitGlfwGL();
    int screenW, screenH;
    glfwGetFramebufferSize(window, &screenW, &screenH);
    s_splitterCursor = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    defer(glfwDestroyCursor(s_splitterCursor));

    s_orbitCam.distance = 5;
    s_orbitCam.heading = 0;
    s_orbitCam.pitch = 0;
    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int /*scanCode*/, int action, int /*mods*/)
    {
        if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);
    });
    glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int /*mods*/)
    {
        if (ImGui::GetIO().WantCaptureMouse) {
            s_mousePressed = false;
            s_draggingSplitter = false;
            return;
        }
        if(button == GLFW_MOUSE_BUTTON_1)
        {
            s_mousePressed = action == GLFW_PRESS;
            s_draggingSplitter = s_mousePressed && hoveringSplitter(window);
        }
    });
    glfwSetCursorPosCallback(window, [](GLFWwindow* window, double x, double y)
    {
        if(s_mousePressed) {
            int windowW, windowH;
            glfwGetWindowSize(window, &windowW, &windowH);
            if(s_draggingSplitter) {
                x = tl::clamp(x, 0., double(windowW));
                s_splitterPercent = x / double(windowW);
            }
            else {
                const glm::vec2 d = glm::vec2{x, y} - s_prevMouse;
                s_orbitCam.applyMouseDrag(d, {windowW, windowH});
                if(x != 0)
                    s_numFramesWithoutChanging = 0;
            }
        }
        const bool showSplitterCursor = s_draggingSplitter || hoveringSplitter(window);
        if(showSplitterCursor) {
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
            glfwSetCursor(window, s_splitterCursor);
        }
        else {
            ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
        }
        s_prevMouse = {x, y};
    });
    glfwSetScrollCallback(window, [](GLFWwindow* /*window*/, double /*dx*/, double dy)
    {
        if (ImGui::GetIO().WantCaptureMouse)
            return;
        s_orbitCam.applyMouseWheel(dy);
        if(dy != 0)
            s_numFramesWithoutChanging = 0;
    });
    glfwSetWindowSizeCallback(window, [](GLFWwindow* /*window*/, int width, int height)
    {
        s_numFramesWithoutChanging = 0;
        for(int i = 0; i < 2; i++) {
            glBindTexture(GL_TEXTURE_2D, s_rt[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
        }
        glBindRenderbuffer(GL_RENDERBUFFER, s_depthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
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

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    // init FBO
    glGenFramebuffers(1, &s_fbo);
    glGenTextures(2, s_rt);
    glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
    for(int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, s_rt[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, screenW, screenH, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, s_rt[i], 0);
    }
    {
        glGenRenderbuffers(1, &s_depthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, s_depthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, screenW, screenH);
        //glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, s_depthRbo);
    }
    GLenum fboDrawBuffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(tl::size(fboDrawBuffers), fboDrawBuffers);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        printf("The framebuffer is not camplete\n");

    // init textures
    glGenTextures(2, &s_textures.envmap);
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, s_textures.envmap);
        tg::simpleInitCubemapTexture();
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        tg::Img3f img(tg::Img3f::load("autumn_ggx_0.hdr"));
        tg::uploadCubemapTexture(0, img.width(), img.height(), GL_RGB16, GL_RGB, GL_FLOAT, (u8*)img.data());
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    }
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, s_textures.convolution);
        tg::simpleInitCubemapTexture();
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        for(int i = 0; i < 16; i++)
        {
            tl::toStringBuffer(s_buffer, "autumn_ggx_", i, ".hdr");
            tg::Img3f img = tg::Img3f::load(s_buffer);
            if(img.data() == nullptr) {
                if(i == 0) {
                    tl::println("Error loading texture: ", s_buffer);
                    return false;
                }
                break;
            }
            tg::uploadCubemapTexture(i, img.width(), img.height(), GL_RGB16, GL_RGB, GL_FLOAT, (u8*)img.data());
        }
    }
    defer(glDeleteTextures(2, &s_textures.envmap));

    // init screen quad
    glGenVertexArrays(1, &s_screenQuadVao);
    glBindVertexArray(s_screenQuadVao);
    glGenBuffers(1, &s_screenQuadVbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_screenQuadVbo);
    static const float k_screenQuadVertData[] = { // intended to be drawn as GL_TRIANGLE_FAN
        -1,-1,  +1,-1,  +1,+1,  -1,+1
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(k_screenQuadVertData), k_screenQuadVertData, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // init environment cube mesh
    glGenVertexArrays(1, &s_envCubeVao);
    defer(glDeleteVertexArrays(1, &s_envCubeVao));
    glBindVertexArray(s_envCubeVao);
    glGenBuffers(1, &s_envCubeVbo);
    defer(glDeleteVertexArrays(1, &s_envCubeVbo));
    glBindBuffer(GL_ARRAY_BUFFER, s_envCubeVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(tg::k_cubeVerts), tg::k_cubeVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    // init object mesh
    tg::createIcoSphereMesh(s_objVao, s_objVbo, s_objEbo, s_objNumInds, 3);
    defer(
        glDeleteVertexArrays(1, &s_objVao);
        glDeleteBuffers(1, &s_objVbo);
        glDeleteBuffers(1, &s_objEbo);
    );

    // init shaders
    tg::createSimpleCubemapShader(s_envProg,
        s_envShadUnifLocs.modelViewProj, s_envShadUnifLocs.cubemap, s_envShadUnifLocs.gammaExp);
    defer(glDeleteProgram(s_envProg));
    glUseProgram(s_envProg);
    glUniform1i(s_envShadUnifLocs.cubemap, 0);
    glUniform1f(s_envShadUnifLocs.gammaExp, 1.f / 2.2f);

    s_splatProg = glCreateProgram();
    {
        const u32 vertShad = glCreateShader(GL_VERTEX_SHADER);
        defer(glDeleteShader(vertShad));
        const char* vertSrcs[] = {tg::srcs::header, k_splatVertShadSrc};
        glShaderSource(vertShad, 2, vertSrcs, nullptr);
        glCompileShader(vertShad);
        if(const char* errMsg = tg::getShaderCompileErrors(vertShad, s_buffer)) {
            tl::println("Error compiling vertex shader:\n", errMsg);
            return 1;
        }

        const u32 fragShad = glCreateShader(GL_FRAGMENT_SHADER);
        defer(glDeleteShader(fragShad));
        const char* fragSrcs[] = {tg::srcs::header, k_splatFragShadSrc};
        glShaderSource(fragShad, 2, fragSrcs, nullptr);
        glCompileShader(fragShad);
        if(const char* errMsg = tg::getShaderCompileErrors(vertShad, s_buffer)) {
            tl::println("Error compiling fragment shader:\n", errMsg);
            return 1;
        }

        glAttachShader(s_splatProg, vertShad);
        glAttachShader(s_splatProg, fragShad);
        glLinkProgram(s_splatProg);
        if(const char* errMsg = tg::getShaderLinkErrors(s_splatProg, s_buffer)) {
            tl::println("Error linking program:\n", errMsg);
            return 1;
        }
        glDetachShader(s_splatProg, vertShad);
        glDetachShader(s_splatProg, fragShad);

        s_splatUnifLocs.tex = glGetUniformLocation(s_splatProg, "u_tex");
    }
    defer(glDeleteProgram(s_splatProg));

    s_iblProg = glCreateProgram();
    s_rtProg = glCreateProgram();
    {
        const char* vertSrcs[] = { tg::srcs::header, k_vertShadSrc };
        const char* fragSrcs[] = { tg::srcs::header, k_fragShadSrc };
        const char* rtFragSrcs[] = {
            tg::srcs::header,
            tg::srcs::hammersley,
            tg::srcs::pcg_uvec3_uvec3,
            tg::srcs::uniformSample,
            tg::srcs::importanceSampleGgxD,
            k_rtFragShadSrc};
        constexpr int numVertSrcs = tl::size(vertSrcs);
        constexpr int numFragSrcs = tl::size(fragSrcs);
        constexpr int numRtFragSrcs = tl::size(rtFragSrcs);
        int srcsSizes[tl::max(numVertSrcs, numFragSrcs, numRtFragSrcs)];

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

        glAttachShader(s_iblProg, vertShad);
        glAttachShader(s_iblProg, fragShad);
        glLinkProgram(s_iblProg);
        if(const char* errMsg = tg::getShaderLinkErrors(s_iblProg, s_buffer)) {
            tl::println("Error linking program:\n", errMsg);
            return 1;
        }

        u32 rtFragShad = glCreateShader(GL_FRAGMENT_SHADER);
        defer(glDeleteShader(rtFragShad));
        for(int i = 0; i < numRtFragSrcs; i++)
            srcsSizes[i] = strlen(rtFragSrcs[i]);
        glShaderSource(rtFragShad, numRtFragSrcs, rtFragSrcs, srcsSizes);
        glCompileShader(rtFragShad);
        if(const char* errMsg = tg::getShaderCompileErrors(rtFragShad, s_buffer)) {
            tl::println("Error compiling RT frament shader:\n", errMsg);
            return 1;
        }

        glAttachShader(s_rtProg, vertShad);
        glAttachShader(s_rtProg, rtFragShad);
        glLinkProgram(s_rtProg);
        if(const char* errMsg = tg::getShaderLinkErrors(s_rtProg, s_buffer)) {
            tl::println("Error compiling frament shader:\n", errMsg);
            return 1;
        }

        { // collect unif locs
            CommonUnifLocs* commonUnifLocs[2] = { &s_iblUnifLocs, &s_rtUnifLocs };
            u32 progs[2] = { s_iblProg, s_rtProg };
            for(int i = 0; i < 2; i++) {
                *commonUnifLocs[i] = {
                    glGetUniformLocation(progs[i], "u_camPos"),
                    glGetUniformLocation(progs[i], "u_model"),
                    glGetUniformLocation(progs[i], "u_modelViewProj"),
                    glGetUniformLocation(progs[i], "u_albedo"),
                    glGetUniformLocation(progs[i], "u_rough2"),
                    glGetUniformLocation(progs[i], "u_metallic"),
                    glGetUniformLocation(progs[i], "u_F0"),
                    glGetUniformLocation(progs[i], "u_convolutedEnv"),
                    glGetUniformLocation(progs[i], "u_lut"),
                };
            }
            s_rtUnifLocs.numSamples = glGetUniformLocation(s_rtProg, "u_numSamples");
            s_rtUnifLocs.numFramesWithoutChanging = glGetUniformLocation(s_rtProg, "u_numFramesWithoutChanging");
            s_rtUnifLocs.fresnelTerm = glGetUniformLocation(s_rtProg, "u_fresnelTerm");
            s_rtUnifLocs.geomTerm = glGetUniformLocation(s_rtProg, "u_geomTerm");
            s_rtUnifLocs.ndfTerm = glGetUniformLocation(s_rtProg, "u_ndfTerm");
            s_rtUnifLocs.test = glGetUniformLocation(s_rtProg, "u_testUnif");
        }
    }
    defer(glDeleteProgram(s_rtProg));
    defer(glDeleteProgram(s_iblProg));

    glEnable(GL_SCISSOR_TEST);

    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        glfwGetFramebufferSize(window, &screenW, &screenH);
        glViewport(0, 0, screenW, screenH);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        drawGui();

        const float aspectRatio = float(screenW) / screenH;
        int splitterX = screenW * s_splitterPercent;
        splitterX = tl::max(0, splitterX - 1);
        int splitterLineWidth = tl::min(1, screenW - splitterX);

        const glm::mat4 viewMtx = tg::calcOrbitCameraMtx(vec3(0, 0, 0), s_orbitCam.heading, s_orbitCam.pitch, s_orbitCam.distance);
        const glm::mat4 projMtx = glm::perspective(glm::radians(45.f), aspectRatio, 0.1f, 1000.f);

        auto uploadCommonUniforms = [&](const CommonUnifLocs& unifLocs)
        {
            const glm::mat4 viewProjMtx = projMtx * viewMtx;
            const glm::mat4 modelMtx(1);
            const glm::vec4 camPos4 = glm::affineInverse(viewMtx) * glm::vec4(0,0,0,1);
            glUniform3fv(unifLocs.camPos, 1, &camPos4[0]);
            glUniformMatrix4fv(unifLocs.model, 1, GL_FALSE, &modelMtx[0][0]);
            glUniformMatrix4fv(unifLocs.modelViewProj, 1, GL_FALSE, &viewProjMtx[0][0]);
            if(unifLocs.albedo != -1)
                glUniform3fv(unifLocs.albedo, 1, &s_albedo[0]);
            if(unifLocs.rough2 != -1)
                glUniform1f(unifLocs.rough2, s_rough*s_rough);
            if(unifLocs.metallic != -1)
                glUniform1f(unifLocs.metallic, s_metallic);
            const glm::vec3 ironF0(0.56f, 0.57f, 0.58f);
            if(unifLocs.F0 != -1)
                glUniform3fv(unifLocs.F0, 1, &ironF0[0]);
            if(unifLocs.convolutedEnv != -1)
                glUniform1i(unifLocs.convolutedEnv, 1);
        };

        // start drawing to render targets
        glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE_MINUS_CONSTANT_ALPHA, GL_CONSTANT_ALPHA); // this is for accumulating frames to make samo sort of temporal antialiasing
        glBlendColor(0, 0, 0, float(s_numFramesWithoutChanging) / (1 + s_numFramesWithoutChanging));
        glScissor(0, 0, screenW, screenH);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glClear(GL_DEPTH_BUFFER_BIT);

        /*{ // draw with prefiltered textures
            glScissor(0, 0, splitterX, screenH);
            glUseProgram(s_iblProg);
            uploadCommonUniforms(s_iblUnifLocs);
            glBindVertexArray(s_objVao);
            glDrawElements(GL_TRIANGLES, s_objNumInds, GL_UNSIGNED_INT, nullptr);
            glClearColor(1, 0, 0, 1);
            //glClear(GL_COLOR_BUFFER_BIT);
        }*/

        glEnable(GL_CULL_FACE);
        { // draw with uniform sampling and importance sampling
            if(s_numFramesWithoutChanging == 0) {
                glClearColor(0,0,0,0);
                glClear(GL_COLOR_BUFFER_BIT);
            }
            glUseProgram(s_rtProg);
            uploadCommonUniforms(s_rtUnifLocs);
            glUniform1ui(s_rtUnifLocs.numSamples, s_numSamplesPerFrame);
            glUniform1ui(s_rtUnifLocs.numFramesWithoutChanging, s_numFramesWithoutChanging);
            if(s_rtUnifLocs.fresnelTerm != -1)
                glUniform1f(s_rtUnifLocs.fresnelTerm, s_enableFresnelTerm);
            if(s_rtUnifLocs.geomTerm != -1)
                glUniform1f(s_rtUnifLocs.geomTerm, s_enableGeomTerm);
            if(s_rtUnifLocs.ndfTerm != -1)
                glUniform1f(s_rtUnifLocs.ndfTerm, s_enableNdfTerm);
            if(s_rtUnifLocs.test != -1)
                glUniform1f(s_rtUnifLocs.test, s_testUnif);
            glBindVertexArray(s_objVao);
            glDrawElements(GL_TRIANGLES, s_objNumInds, GL_UNSIGNED_INT, nullptr);
        }

        glDisable(GL_CULL_FACE);
        // now we start drawing to the screen
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // draw background
        glDisable(GL_DEPTH_TEST); // no reading, no writing
        glDisable(GL_BLEND);
        {
            glm::mat4 viewMtxWithoutTranslation = viewMtx;
            viewMtxWithoutTranslation[3][0] = viewMtxWithoutTranslation[3][1] = viewMtxWithoutTranslation[3][2] = 0;
            const glm::mat4 viewProjMtx = projMtx * viewMtxWithoutTranslation;
            glUseProgram(s_envProg);
            glUniformMatrix4fv(s_envShadUnifLocs.modelViewProj, 1, GL_FALSE, &viewProjMtx[0][0]);
            glBindVertexArray(s_envCubeVao);
            glDrawArrays(GL_TRIANGLES, 0, 6*6);
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // draw two sides of the splitter
        glUseProgram(s_splatProg);
        glUniform1i(s_splatUnifLocs.tex, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(s_screenQuadVao);
        // draw left side of the splitter
        glScissor(0, 0, splitterX, screenH);
        glBindTexture(GL_TEXTURE_2D, s_rt[0]);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        // draw right side of the splitter
        glScissor(splitterX+splitterLineWidth, 0, screenW - (splitterX+splitterLineWidth), screenH);
        glBindTexture(GL_TEXTURE_2D, s_rt[1]);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        // draw splitter line
        {
            glScissor(splitterX, 0, splitterLineWidth, screenH);
            glClearColor(0, 1, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        s_numFramesWithoutChanging++;

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwWaitEventsTimeout(0.01);
    }

    return true;
}

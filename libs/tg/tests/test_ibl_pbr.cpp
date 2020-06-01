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

static char s_buffer[4*1024];
static OrbitCameraInfo s_orbitCam;
static struct { u32 envmap, convolution; } s_textures;
static u32 s_envCubeVao, s_envCubeVbo;
static u32 s_objVao, s_objVbo, s_objNumVerts;
static u32 s_envProg, s_iblProg;
static struct { i32 modelViewProj, cubemap, gammaExp; } s_envShadUnifLocs;
static struct { i32 camPos, model, modelViewProj, albedo, rough2, metallic, F0, convolutedEnv, lut; } s_iblUnifLocs;

static const char k_glslVersionSrc[] = "#version 330 core\n\n";

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

bool test_iblPbr()
{
    GLFWwindow* window = simpleInitGlfwGL();
    s_orbitCam.distance = 10;
    s_orbitCam.heading = 0;
    s_orbitCam.pitch = 0;
    addOrbitCameraBaviour(window, s_orbitCam);

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

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
    tg::createCubeMesh(s_objVao, s_objVbo, s_objNumVerts, true);
    defer(
        glDeleteVertexArrays(1, &s_objVao);
        glDeleteBuffers(1, &s_objVao);
    );

    // init shaders
    tg::createSimpleCubemapShader(s_envProg,
        s_envShadUnifLocs.modelViewProj, s_envShadUnifLocs.cubemap, s_envShadUnifLocs.gammaExp);
    defer(glDeleteProgram(s_envProg));
    glUseProgram(s_envProg);
    glUniform1i(s_envShadUnifLocs.cubemap, 0);
    glUniform1f(s_envShadUnifLocs.gammaExp, 1.f / 2.2f);

    s_iblProg = glCreateProgram();
    {
        const char* vertSrcs[] = { k_glslVersionSrc, k_vertShadSrc };
        const char* fragSrcs[] = { k_glslVersionSrc, k_fragShadSrc };
        constexpr int numVertSrcs = tl::size(vertSrcs);
        constexpr int numFragSrcs = tl::size(fragSrcs);
        int srcsSizes[tl::max(numVertSrcs, numVertSrcs)];

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

        s_iblUnifLocs = {
            glGetUniformLocation(s_iblProg, "u_camPos"),
            glGetUniformLocation(s_iblProg, "u_model"),
            glGetUniformLocation(s_iblProg, "u_modelViewProj"),
            glGetUniformLocation(s_iblProg, "u_albedo"),
            glGetUniformLocation(s_iblProg, "u_rough2"),
            glGetUniformLocation(s_iblProg, "u_metallic"),
            glGetUniformLocation(s_iblProg, "u_F0"),
            glGetUniformLocation(s_iblProg, "u_convolutedEnv"),
            glGetUniformLocation(s_iblProg, "u_lut"),
        };
    }
    defer(glDeleteProgram(s_iblProg));

    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        int screenW, screenH;
        glfwGetFramebufferSize(window, &screenW, &screenH);
        glViewport(0, 0, screenW, screenH);
        glScissor(0, 0, screenW, screenH);
        const float aspectRatio = float(screenW) / screenH;

        glClear(GL_DEPTH_BUFFER_BIT);

        const glm::mat4 viewMtx = tg::calcOrbitCameraMtx(s_orbitCam.heading, s_orbitCam.pitch, s_orbitCam.distance);
        const glm::mat4 projMtx = glm::perspective(glm::radians(45.f), aspectRatio, 0.1f, 1000.f);

        glDisable(GL_DEPTH_TEST); // no reading, no writing
        {
            glm::mat4 viewMtxWithoutTranslation = viewMtx;
            viewMtxWithoutTranslation[3][0] = viewMtxWithoutTranslation[3][1] = viewMtxWithoutTranslation[3][2] = 0;
            const glm::mat4 viewProjMtx = projMtx * viewMtxWithoutTranslation;
            glUseProgram(s_envProg);
            glUniformMatrix4fv(s_envShadUnifLocs.modelViewProj, 1, GL_FALSE, &viewProjMtx[0][0]);
            glBindVertexArray(s_envCubeVao);
            glDrawArrays(GL_TRIANGLES, 0, 6*6);
        }

        glEnable(GL_DEPTH_TEST);

        const glm::mat4 viewProjMtx = projMtx * viewMtx;
        const glm::mat4 modelMtx(1);
        glUseProgram(s_iblProg);
        const glm::vec4 camPos4 = glm::affineInverse(viewMtx) * glm::vec4(0,0,0,1);
        glUniform3fv(s_iblUnifLocs.camPos, 1, &camPos4[0]);
        glUniformMatrix4fv(s_iblUnifLocs.model, 1, GL_FALSE, &modelMtx[0][0]);
        glUniformMatrix4fv(s_iblUnifLocs.modelViewProj, 1, GL_FALSE, &viewProjMtx[0][0]);
        const glm::vec3 albedo(0.5, 0.5, 0.5);
        if(s_iblUnifLocs.albedo != -1)
            glUniform3fv(s_iblUnifLocs.albedo, 1, &albedo[0]);
        if(s_iblUnifLocs.rough2 != -1)
            glUniform1f(s_iblUnifLocs.rough2, 0.2f);
        const glm::vec3 ironF0(0.56f, 0.57f, 0.58f);
        if(s_iblUnifLocs.F0 != -1)
            glUniform3fv(s_iblUnifLocs.F0, 1, &ironF0[0]);
        if(s_iblUnifLocs.convolutedEnv != -1)
            glUniform1i(s_iblUnifLocs.convolutedEnv, 1);
        glBindVertexArray(s_objVao);
        glDrawArrays(GL_TRIANGLES, 0, 6*6);

        glfwSwapBuffers(window);
        glfwWaitEventsTimeout(0.01);
    }

    return true;
}

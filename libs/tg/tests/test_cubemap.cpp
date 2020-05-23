#include <stdio.h>
#include <assert.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <tg/shader_utils.hpp>
#include <tg/cameras.hpp>
#include <tg/geometry_utils.hpp>
#include <tg/mesh_utils.hpp>
#include <tg/texture_utils.hpp>
#include <tl/defer.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/transform.hpp>
#include <stbi.h>
#include "test_utils.hpp"

constexpr float PI = glm::pi<float>();

static GLFWwindow* window;

static const char s_vertShadSrc[] =
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

static const char s_fragShadSrc[] =
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

struct UnifLocs {
    i32 modelViewProj;
    i32 cubemap;
} unifLocs;

static char s_buffer[4*1024];

static OrbitCameraInfo s_orbitCam;

static u32 s_environmentTextureUnit = 1;
static u32 s_cubeTextureUnit = 0;
static bool s_seamlessCubemapEnabled = false;
static bool s_seamlessCubemapEnabledChanged = false;
void onKey(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_1 && action == GLFW_PRESS)
        s_environmentTextureUnit ^= 1;
    if (key == GLFW_KEY_2 && action == GLFW_PRESS)
        s_cubeTextureUnit ^= 1;
    if(key == GLFW_KEY_S && action == GLFW_PRESS) {
        s_seamlessCubemapEnabled = s_seamlessCubemapEnabled != true;
        s_seamlessCubemapEnabledChanged = true;
        (s_seamlessCubemapEnabled ? glEnable : glDisable)(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    }
}

bool test_cubemap()
{
    window = simpleInitGlfwGL();
    if(!window)
        return 1;

    glfwSetKeyCallback(window, onKey);

    s_orbitCam.heading = s_orbitCam.pitch = 0;
    s_orbitCam.distance = 4;
    addOrbitCameraBaviour(window, s_orbitCam);

    u32 cubemapTextures[2];
    glGenTextures(2, cubemapTextures);
    defer(glDeleteTextures(2, cubemapTextures));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTextures[0]);
    {
        int w, h, nc;
        u8* data = stbi_load("cubemap_test_rgb.png", &w, &h, &nc, 3);
        if(data) {
            tg::uploadCubemapTexture(0, w, h, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else {
            printf("error loading img\n");
            return 1;
        }
    }

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTextures[1]);
    {
        int w, h, nc;
        u8* data = stbi_load("test.hdr", &w, &h, &nc, 3);
        if(data) {
            tg::uploadCubemapTexture(0, w, h, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else {
            printf("error loading img\n");
            return 1;
        }
    }

    u32 prog = glCreateProgram();
    defer(glDeleteProgram(prog));
    {
        u32 vertShad = glCreateShader(GL_VERTEX_SHADER);
        defer(glDeleteShader(vertShad));
        const char* vertShadSrc = s_vertShadSrc;
        glShaderSource(vertShad, 1, &vertShadSrc, nullptr);
        glCompileShader(vertShad);
        if(const char* errorMsg = tg::getShaderCompileErrors(vertShad, s_buffer)) {
            printf("Error compiling vertex shader:\n%s\n", errorMsg);
            return 0;
        }

        u32 fragShad = glCreateShader(GL_FRAGMENT_SHADER);
        defer(glDeleteShader(fragShad));
        const char* fragShadSrc = s_fragShadSrc;
        glShaderSource(fragShad, 1, &fragShadSrc, nullptr);
        glCompileShader(fragShad);
        if(const char* errorMsg = tg::getShaderCompileErrors(fragShad, s_buffer)) {
            printf("Error compiling fragment shader:\n%s\n", errorMsg);
            return 0;
        }

        glAttachShader(prog, vertShad);
        glAttachShader(prog, fragShad);
        glLinkProgram(prog);
        if(const char* errorMsg = tg::getShaderLinkErrors(prog, s_buffer)) {
            printf("Error linking:\n%s\n", errorMsg);
            return 0;
        }
    }

    glUseProgram(prog);
    unifLocs.modelViewProj = glGetUniformLocation(prog, "u_modelViewProjMat");
    unifLocs.cubemap = glGetUniformLocation(prog, "u_cubemap");
    glUniform1i(unifLocs.cubemap, 0);

    u32 vao, vbo;
    glGenVertexArrays(1, &vao);
    defer(glDeleteVertexArrays(1, &vao));
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    defer(glDeleteVertexArrays(1, &vbo));
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(tg::cubeVerts), tg::cubeVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // draw scene
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glScissor(0, 0, w, h);

        glClearColor(0, 0.2f, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const glm::mat4 modelMtx(1);
        const glm::mat4 projMtx = glm::perspective(glm::radians(45.f), float(w)/h, 0.1f, 1000.f);
        const glm::mat4 viewMtx = tg::calcOrbitCameraMtx(s_orbitCam.heading, s_orbitCam.pitch, s_orbitCam.distance);

        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        { // draw environment
            glm::mat4 viewMtxWithoutTranslation = viewMtx;
            viewMtxWithoutTranslation[3][0] = viewMtxWithoutTranslation[3][1] = viewMtxWithoutTranslation[3][2] = 0;
            const glm::mat4 modelViewProj = projMtx * viewMtxWithoutTranslation * modelMtx;
            glUniformMatrix4fv(unifLocs.modelViewProj, 1, GL_FALSE, &modelViewProj[0][0]);
            glUniform1i(unifLocs.cubemap, s_environmentTextureUnit);
            glDrawArrays(GL_TRIANGLES, 0, 6*6);
        }

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        { // draw cube
            const glm::mat4 modelViewProj = projMtx * viewMtx * modelMtx;
            glUniformMatrix4fv(unifLocs.modelViewProj, 1, GL_FALSE, &modelViewProj[0][0]);
            glUniform1i(unifLocs.cubemap, s_cubeTextureUnit);
            glDrawArrays(GL_TRIANGLES, 0, 6*6);
        }

        glfwSwapBuffers(window);
        glfwWaitEventsTimeout(0.01);
    }

    return true;
}

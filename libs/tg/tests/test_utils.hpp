#pragma once

#include <glm/mat3x3.hpp>
#include <tl/int_types.hpp>

struct GLFWwindow;

GLFWwindow* simpleInitGlfwGL();

extern char g_buffer[4*1024];

struct OrbitCameraInfo {
    float heading, pitch;
    float distance;
    void applyMouseDrag(glm::vec2 deltaPixels, glm::vec2 screenSize);
    void applyMouseWheel(float dy);
};

void addSimpleOrbitCameraBaviour(GLFWwindow* window, OrbitCameraInfo& orbitCamInfo);

extern const char g_cubemapVertShadSrc[];
extern const char g_cubemapFragShadSrc[];
struct SimpleCubemapShaderUnifLocs {
    i32 modelViewProj;
    i32 cubemap;
};
void createSimpleCubemapShader(u32& prog, SimpleCubemapShaderUnifLocs& unifLocs);

#pragma once

#include <glm/mat3x3.hpp>

struct GLFWwindow;

GLFWwindow* simpleInitGlfwGL();

struct OrbitCameraInfo {
    float heading, pitch;
    float distance;
};
void addOrbitCameraBaviour(GLFWwindow* window, OrbitCameraInfo& orbitCamInfo);

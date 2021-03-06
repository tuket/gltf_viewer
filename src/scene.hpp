#pragma once

#include <tl/int_types.hpp>

struct GLFWwindow;

namespace mouse_handling
{
void onMouseButton(GLFWwindow* window, int button, int action, int mods);
void onMouseMove(GLFWwindow* window, double x, double y);
void onMouseWheel(GLFWwindow* window, double dx, double dy);
}

void createAxesMesh();
void createFloorGridMesh();
void createCrosshairMesh();
void createBasicTextures();

void update(float dt);
void drawScene();
void drawGui();

bool loadGltf(const char* path);
void onFileDroped(GLFWwindow* window, int count, const char** paths);

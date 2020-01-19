#pragma once

#include <tl/int_types.hpp>

struct GLFWwindow;

void drawScene();
void drawGui();

bool loadGltf(const char* path);
void onFileDroped(GLFWwindow* window, int count, const char** paths);

extern i32 selectedSceneInd;

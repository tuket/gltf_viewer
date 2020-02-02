#pragma once

#include <tl/str.hpp>
#include <cgltf.h>
#include <glm/mat4x4.hpp>
#include <glm/gtc/constants.hpp>
#include <glad/glad.h>

constexpr float PI = glm::pi<float>();

constexpr int SCRATCH_BUFFER_SIZE = 1*1024*1024;
union ScratchBuffer {
    u8 buffer[SCRATCH_BUFFER_SIZE];
    char str[SCRATCH_BUFFER_SIZE];
};
extern ScratchBuffer scratch;

enum class EAttrib : u8 {
    POSITION = 0,
    NORMAL,
    TANGENT,
    COTANGENT,
    TEXCOORD_0,
    TEXCOORD_1,
    COLOR,
    JOINTS,
    WEIGHTS,
    COUNT
};
CStr toStr(EAttrib type);
EAttrib strToEAttrib(CStr str);

struct OrbitCameraInfo {
    float heading, pitch; // in radians
    float distance;
    glm::mat4 viewMtx()const;
};

struct CameraProjectionInfo {
    float fovY;
    float nearDist, farDist;
};

bool Splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.0f);

i32 cgltfTypeNumComponents(cgltf_type type);
GLenum cgltfComponentTypeToGl(cgltf_component_type type);
GLenum cgltfPrimTypeToGl(cgltf_primitive_type type);

const char* cgltfPrimitiveTypeStr(cgltf_primitive_type type);
const char* cgltfTypeStr(cgltf_type type);
const char* cgltfComponentTypeStr(cgltf_component_type type);
const char* cgltfAttribTypeStr(cgltf_attribute_type type);
const char* cgltfCameraTypeStr(cgltf_camera_type type);
const char* cgltfValueStr(cgltf_type type, const cgltf_float (&m)[16]);

const char* glMinFilterModeStr(int minFilterMode);
const char* glMagFilterModeStr(int magFitlerMode);
const char* glTextureWrapModeStr(int wrapMode);

/*
struct ImFont;
namespace fonts
{
extern ImFont* roboto;
extern ImFont* robotoBold;
}
*/

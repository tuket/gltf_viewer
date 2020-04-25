#pragma once

#include <tl/str.hpp>
#include <cgltf.h>
#include <glm/mat4x4.hpp>
#include <glm/gtc/constants.hpp>
#include <glad/glad.h>
#include <tl/span.hpp>

constexpr float PI = glm::pi<float>();

class ScratchBuffer {
public:
    ScratchBuffer();
    u8* data() { return _data; } // returns 16 byte aligned ptr
    size_t size()const { return _size; }
    void growIfNeeded(size_t size);
    template <typename T>
    T* ptr() {return reinterpret_cast<T*>(_data); }
    template <typename T>
    tl::Span<T> asArray() {
        return tl::Span<T>(reinterpret_cast<T*>(_data), _size / sizeof(T));
    }
    char* str() { return ptr<char>(); }
private:
    size_t _size;
    u8* _nonAlignedData;
    u8* _data;
};
extern ScratchBuffer scratch;
static inline tl::Span<char> scratchStr() { return scratch.asArray<char>(); }
static inline tl::Span<u8> scratchU8() { return scratch.asArray<u8>(); }

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

enum class ETexUnit : u8 {
    ALBEDO,
    NORMAL,
    PHYSICS,
    COUNT
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

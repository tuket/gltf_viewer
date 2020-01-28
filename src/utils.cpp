#include "utils.hpp"

#include <glm/vec2.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <tl/basic.hpp>
#include <glad/glad.h>
#include <glm/gtx/euler_angles.hpp>

ScratchBuffer scratch;

glm::mat4 OrbitCameraInfo::viewMtx()const
{
    auto mtx = glm::eulerAngleXY(-PI*pitch, -PI*heading);
    mtx[3][2] = -distance;
    return mtx;
}

bool Splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size)
{
    using namespace ImGui;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiID id = window->GetID("##Splitter");
    ImRect bb;
    bb.Min = glm::vec2(window->DC.CursorPos) + (split_vertically ? glm::vec2(*size1, 0.0f) : glm::vec2(0.0f, *size1));
    bb.Max =
        glm::vec2(bb.Min) +
        glm::vec2(CalcItemSize(split_vertically ?
            glm::vec2(thickness, splitter_long_axis_size) : glm::vec2(splitter_long_axis_size, thickness),
            0.0f, 0.0f
        ));
    return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 0.0f);
}

const char* cgltfPrimitiveTypeStr(cgltf_primitive_type type)
{
    static const char* strs[] = {
        "POINTS",
        "LINES",
        "LINE_LOOP",
        "LINE_STRIP",
        "TRIANGLES",
        "TRIANGLE_STRIP",
        "TRIANGLE_FAN"
    };
    const u32 i = type;
    assert(i < tl::size(strs));
    return strs[i];
}

const char* cgltfTypeStr(cgltf_type type)
{
    static const char* strs[] = {"invalid", "scalar", "vec2", "vec3", "vec4", "mat2", "mat3", "mat4"};
    const u32 i = type;
    assert(i < tl::size(strs));
    return strs[i];
}

const char* cgltfComponentTypeStr(cgltf_component_type type)
{
    static const char* strs[] = {"invalid", "i8", "u8", "i16", "u16", "u32", "f32"};
    const u32 i = type;
    assert(i < tl::size(strs));
    return strs[i];
}

const char* cgltfAttribTypeStr(cgltf_attribute_type type)
{
    static const char* strs[] = {"invalid", "position", "normal", "tangent", "texcoord", "color", "joints", "weights"};
    const u32 i = type;
    assert(i < tl::size(strs));
    return strs[i];
}

const char* cgltfCameraTypeStr(cgltf_camera_type type)
{
    static const char* strs[] = {"invalid", "perspective", "orthographic"};
    const u32 i = type;
    assert(i < tl::size(strs));
    return strs[i];
}

const char* cgltfValueStr(cgltf_type type, const cgltf_float (&m)[16])
{
    switch(type)
    {
    case cgltf_type_scalar:
        sprintf(scratch.str, "%f", m[0]);
        break;
    case cgltf_type_vec2:
        sprintf(scratch.str, "{%f, %f}", m[0], m[1]);
        break;
    case cgltf_type_vec3:
        sprintf(scratch.str, "{%f, %f, %f}", m[0], m[1], m[2]);
        break;
    case cgltf_type_vec4:
        sprintf(scratch.str, "{%f, %f, %f, %f}", m[0], m[1], m[2], m[3]);
        break;
    case cgltf_type_mat2:
        sprintf(scratch.str, "{{%f, %f}, {%f, %f}}", m[0], m[1], m[2], m[3]);
        break;
    case cgltf_type_mat3:
        sprintf(scratch.str, "{{%f, %f, %f}, {%f, %f, %f}, {%f, %f, %f}}", m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
        break;
    case cgltf_type_mat4:
        sprintf(scratch.str, "{{%f, %f, %f, %f}, {%f, %f, %f, %f}, {%f, %f, %f, %f}, {%f, %f, %f, %f}}", m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
        break;
    default:
        sprintf(scratch.str, "invalid");
    }
    return scratch.str;
}

const char* glMinFilterModeStr(int minFilterMode)
{
    switch(minFilterMode) {
        case GL_NEAREST: return "nearest pixel";
        case GL_LINEAR: return "linear pixel";
        case GL_NEAREST_MIPMAP_NEAREST: return "nearest pixel, nearest mipmap";
        case GL_LINEAR_MIPMAP_NEAREST: return "linear pixel, nearest mipmap";
        case GL_NEAREST_MIPMAP_LINEAR: return "nearest pixel, linear mipmap";
        case GL_LINEAR_MIPMAP_LINEAR: return "linear pixel, linear mipmap";
    }
    assert(false);
    return "";
}

const char* glMagFilterModeStr(int magFitlerMode)
{
    switch(magFitlerMode) {
        case GL_NEAREST: return "nearest";
        case GL_LINEAR: return "linear";
    }
    assert(false);
    return "";
}

const char* glTextureWrapModeStr(int wrapMode)
{
    switch (wrapMode) {
        case GL_CLAMP_TO_EDGE: return "clamp to edge";
        case GL_CLAMP_TO_BORDER: return "clamp to border";
        case GL_MIRRORED_REPEAT: return "mirrored repeat";
        case GL_REPEAT: return "repeat";
        //case GL_MIRROR_CLAMP_TO_EDGE: return "mirror clamp to edge";
    }
    assert(false);
    return "";
}

/*
namespace fonts
{
ImFont* roboto = nullptr;
ImFont* robotoBold = nullptr;
}
*/

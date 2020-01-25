#include "utils.hpp"

#include <glm/vec2.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <tl/basic.hpp>
#include <glad/glad.h>

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

CStr cgltfPrimitiveTypeStr(cgltf_primitive_type type)
{
    static CStr strs[] = {
        "POINTS",
        "LINES",
        "LINE_LOOP",
        "LINE_STRIP",
        "TRIANGLES",
        "TRIANGLE_STRIP",
        "TRIANGLE_FAN"
    };
    const u32 i = type;
    assert(i < size(strs));
    return strs[i];
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

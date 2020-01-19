#include "utils.hpp"

#include <glm/vec2.hpp>
#include <imgui.h>
#include <imgui_internal.h>

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

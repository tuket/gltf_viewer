#include "scene.hpp"

#include <glad/glad.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <cgltf.h>
#include <stdio.h>
#include <tl/int_types.hpp>
#include <tl/fmt.hpp>
#include <tl/containers/vector.hpp>
#include "utils.hpp"

constexpr int SCRATCH_BUFFER_SIZE = 1*1024*1024;
static union {
    u8 scratchBuffer[SCRATCH_BUFFER_SIZE];
    char scratchStr[SCRATCH_BUFFER_SIZE];
};
static cgltf_data* parsedData = nullptr;
static cgltf_node* selectedNode = nullptr;
static float leftPanelSize = 0.3f, rightPanelSize = 1.f - leftPanelSize;

static u32 shaderProg = 0;

// scene gou resources
static u32 numVbos = 0;
static u32 vbos[128];
static u32 numEbos = 0;
static u32 ebos[128];
static u32 numVaos = 0;
static u32 vaos[128];
static u32 numTextures = 0;
static u32 textures[128];

void freeSceneGpuResources()
{
    glDeleteBuffers(numVbos, vbos);
    glDeleteBuffers(numEbos, ebos);
    glDeleteBuffers(numVaos, vaos);
    glDeleteTextures(numTextures, textures);
}

i32 selectedSceneInd = 0;

void drawScene()
{
    glClearColor(0.1f, 0.2f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if(!parsedData)
        return;

    //parsedData->
}

void sceneNodeGuiRecusive(cgltf_node* node)
{
    ImGuiTreeNodeFlags flags = 0;
    if(selectedNode == node)
        flags |= ImGuiTreeNodeFlags_Selected;

    if(node->children_count) {
        flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if(ImGui::TreeNodeEx((void*)node, flags, "%s", node->name))
        {
            if(ImGui::IsItemClicked())
                    selectedNode = node;
            for(size_t i = 0; i < node->children_count; i++)
                sceneNodeGuiRecusive(node->children[i]);
            ImGui::TreePop();
        }
    }
    else {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        ImGui::TreeNodeEx((void*)node, flags, "%s", node->name);
        if(ImGui::IsItemClicked())
                selectedNode = node;
    }
}

void drawGui()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::Begin("drag & drop a glTF file");
    ImGui::PopStyleVar();

    if(!parsedData) {
        ImGui::End();
        return;
    }

    const float W = ImGui::GetWindowSize().x;
    leftPanelSize *= W;
    rightPanelSize *= W;
    Splitter(true, 4, &leftPanelSize, &rightPanelSize, 0, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
    ImGui::BeginChild("left_panel", {leftPanelSize, -1}, true, ImGuiWindowFlags_AlwaysUseWindowPadding);
    auto sceneComboDisplayStr = [](i32 sceneInd) -> CStr {
        const char* sceneName = "";
        if(sceneInd != -1) {
            sceneName = parsedData->scenes[sceneInd].name;
            sceneName = sceneName ? sceneName : "";
            tl::toStringBuffer(scratchStr, sceneInd, ") ", sceneName);
        }
        return scratchStr;
    };
    if(ImGui::BeginCombo("scene", sceneComboDisplayStr(selectedSceneInd)))
    {
        for(i32 sceneInd = 0; sceneInd < (i32)parsedData->scenes_count; sceneInd++)
        {
            const auto& scene = parsedData->scenes[sceneInd];
            ImGui::PushID((void*)&scene);
            if (ImGui::Selectable(sceneComboDisplayStr(sceneInd)))
                selectedSceneInd = sceneInd;
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    if(selectedSceneInd != -1)
    {
        const cgltf_scene& selectedScene = parsedData->scenes[selectedSceneInd];
        for(size_t nodeInd = 0; nodeInd < selectedScene.nodes_count; nodeInd++)
        if(selectedScene.nodes[nodeInd]->parent == nullptr)
            sceneNodeGuiRecusive(selectedScene.nodes[nodeInd]);
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("right_panel", {rightPanelSize, -1}, true);
    ImGui::Text("be");
    ImGui::EndChild();

    ImGui::PopStyleVar(1);

    leftPanelSize /= ImGui::GetWindowSize().x;
    rightPanelSize /= ImGui::GetWindowSize().x;

    ImGui::End();
}

bool loadGltf(const char* path)
{
    cgltf_data* newParsedData = parsedData;
    cgltf_options options = {};
    cgltf_result result = cgltf_parse_file(&options, path, &newParsedData);
    if(result == cgltf_result_success)
    {
        cgltf_free(parsedData);
        parsedData = newParsedData;
        cgltf_load_buffers(&options, parsedData, path);
        selectedSceneInd = -1;
        for(u32 i = 0; i < parsedData->scenes_count; i++)
            if(parsedData->scene == &parsedData->scenes[i]) {
                selectedSceneInd = i;
                break;
            }

        return true;
    }
    fprintf(stderr, "error loading\n");
    return false;
}

void onFileDroped(GLFWwindow* window, int count, const char** paths)
{
    assert(count);
    const char* path = paths[0];
    loadGltf(path);
}

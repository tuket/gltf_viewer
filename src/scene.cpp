#include "scene.hpp"

#include <glad/glad.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <cgltf.h>
#include <stdio.h>
#include <tl/int_types.hpp>
#include <tl/fmt.hpp>
#include <tl/containers/fvector.hpp>
#include "utils.hpp"

using tl::FVector;

constexpr int SCRATCH_BUFFER_SIZE = 1*1024*1024;
static union {
    u8 scratchBuffer[SCRATCH_BUFFER_SIZE];
    char scratchStr[SCRATCH_BUFFER_SIZE];
};
static CStr openedFilePath = "";
static cgltf_data* parsedData = nullptr;
static cgltf_node* selectedNode = nullptr;

// scene gpu resources
namespace gpu_resources
{
static u32 shaderProg = 0;
static FVector<u32, 128> vbos;
static FVector<u32, 128> ebos;
static FVector<u32, 128> vaos;
static FVector<u32, 128> textures;
}

void freeSceneGpuResources()
{
    using namespace gpu_resources;
    glDeleteBuffers(vbos.size(), vbos.begin());
    glDeleteBuffers(ebos.size(), ebos.begin());
    glDeleteBuffers(vaos.size(), vaos.begin());
    glDeleteTextures(textures.size(), textures.begin());
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
        else {
            if(ImGui::IsItemClicked())
                    selectedNode = node;
        }

    }
    else {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        ImGui::TreeNodeEx((void*)node, flags, "%s", node->name);
        if(ImGui::IsItemClicked())
                selectedNode = node;
    }
}

void drawGui_scenesTab()
{
    static float leftPanelSize = 0.3f, rightPanelSize = 1.f - leftPanelSize;
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
}

void drawGui_meshesTab()
{
    const auto* meshes = parsedData->meshes;
    const size_t numMeshes = parsedData->meshes_count;
    for(size_t i = 0; i < numMeshes; i++)
    {
        auto& mesh = meshes[i];
        tl::toStringBuffer(scratchStr, i, ") ", mesh.name ? mesh.name : "");
        if(ImGui::CollapsingHeader(scratchStr))
        {
            if(ImGui::TreeNode((void*)&mesh, "Primitives (%ld)", mesh.primitives_count))
            {
                const size_t numPrimitives = mesh.primitives_count;
                auto& primitives = mesh.primitives;
                for(size_t primInd = 0; primInd < numPrimitives; primInd++)
                {
                    auto& prim = primitives[primInd];
                    if(ImGui::TreeNode((void*)&prim, "%ld", primInd))
                    {
                        ImGui::Text("Type: %s", cgltfPrimitiveTypeStr(prim.type).c_str());
                        if(prim.indices) {
                            //prim.indices->component_type
                        }
                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }
        }
    }
}

void drawGui_texturesTab()
{
    tl::CArray<cgltf_texture> textures(parsedData->textures, parsedData->textures_count);
    for(size_t i = 0; i < textures.size(); i++)
    {
        auto& texture = textures[i];
        tl::toStringBuffer(scratchStr, i, ") ", texture.name ? texture.name : "");
        if(ImGui::TreeNode(scratchStr))
        {
            ImGui::Text("Name: %s", texture.image->name);
            ImGui::Text("File path: %s", texture.image->uri);
            ImGui::Text("MIME Type: %s", texture.image->mime_type);
            //texture.image->mime_type
            ImGui::TreePop();
        }
    }
}

void drawGui()
{
    if(!parsedData) {
        ImGui::Begin("drag & drop a glTF file##0");
        ImGui::End();
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    tl::toStringBuffer(scratchStr, openedFilePath,"##0");
    ImGui::Begin(scratchStr);
    ImGui::PopStyleVar();

    if (ImGui::BeginTabBar("TopTabBar"))
    {
        if (ImGui::BeginTabItem("Scenes")) {
            drawGui_scenesTab();
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Meshes")) {
            drawGui_meshesTab();
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Textures")) {
            drawGui_texturesTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

bool loadGltf(const char* path)
{
    openedFilePath = path;
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

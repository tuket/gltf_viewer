#include "scene.hpp"

#include <glad/glad.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <cgltf.h>
#include <stdio.h>
#include <tl/int_types.hpp>
#include <tl/basic.hpp>
#include <tl/fmt.hpp>
#include <tl/containers/fvector.hpp>
#include <stbi.h>
#include "utils.hpp"

using tl::FVector;
using tl::Array;
using tl::CArray;

constexpr int SCRATCH_BUFFER_SIZE = 1*1024*1024;
static union {
    u8 scratchBuffer[SCRATCH_BUFFER_SIZE];
    char scratchStr[SCRATCH_BUFFER_SIZE];
};
static CStr openedFilePath = "";
static cgltf_data* parsedData = nullptr;
static cgltf_node* selectedNode = nullptr;

constexpr size_t MAX_BUFFER_OBJS = 256;
constexpr size_t MAX_VERT_ARRAYS = 128;
constexpr size_t MAX_TEXTURES = 128;

// scene gpu resources
namespace gpu
{
static u32 shaderProg = 0;
static FVector<u32, MAX_BUFFER_OBJS> bos;
static FVector<u32, MAX_VERT_ARRAYS> vaos;
static FVector<u32, MAX_TEXTURES> textures;
static glm::ivec2 textureSizes[MAX_TEXTURES];
}

const float MIN_IMGUI_IMG_HEIGHT = 32.f;
const float DEFAULT_IMGUI_IMG_HEIGHT = 128.f;

namespace imgui_state
{
static float textureHeights[MAX_TEXTURES];
}

void freeSceneGpuResources()
{
    using namespace gpu;
    glDeleteBuffers(bos.size(), bos.begin());
    glad_glDeleteVertexArrays(vaos.size(), vaos.begin());
    glDeleteTextures(textures.size(), textures.begin());
}

i32 selectedSceneInd = 0;

size_t getBufferInd(cgltf_buffer* buffer) {
    return (size_t)(buffer - parsedData->buffers);
}
size_t getBufferViewInd(cgltf_buffer_view* view) {
    return (size_t)(view - parsedData->buffer_views);
}

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
            ImGui::TreePush();
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
            ImGui::TreePop();
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
        if(ImGui::CollapsingHeader(scratchStr))
        {
            ImGui::TreePush();
            ImGui::Text("Name: %s", texture.image->name);
            ImGui::Text("File path: %s", texture.image->uri);
            ImGui::Text("MIME Type: %s", texture.image->mime_type);
            ImGui::Text("Size: %dx%d", gpu::textureSizes[i].x, gpu::textureSizes[i].y);
            const float aspectRatio = (float)gpu::textureSizes[i].x / gpu::textureSizes->y;
            float& imguiH = imgui_state::textureHeights[i];
            ImGui::SliderFloat("Scale", &imguiH, MIN_IMGUI_IMG_HEIGHT, gpu::textureSizes[i].y, "%.0f");
            ImGui::Image((void*)(u64)gpu::textures[i], {imguiH * aspectRatio, imguiH});
            // TODO
            /*if(ImGui::BeginPopupContextItem("right-click"))
            {
                if(ImGui::Selectable("copy")) {

                }
                ImGui::EndPopup();
            }*/
            ImGui::TreePop();
        }
    }
}

void drawGui_buffersTab()
{
    CArray<cgltf_buffer> buffers (parsedData->buffers, parsedData->buffers_count);
    for(size_t i = 0; i < buffers.size(); i++)
    {
        const cgltf_buffer& buffer = buffers[i];
        size_t sizeXBytes = buffer.size;
        static const CStr unitsStrs[] = {"bytes", "KB", "MB", "GB"};
        int unitInd = 0;
        while(sizeXBytes > 1024) {
            sizeXBytes /= 1024;
            unitInd++;
        }
        tl::toStringBuffer(scratchStr, i, ") ", buffer.uri ? buffer.uri : "", sizeXBytes, unitsStrs[unitInd]);
        if(ImGui::CollapsingHeader(scratchStr))
        {
            // TODO
        }
    }
}

void drawGui_bufferViewsTab()
{
    CArray<cgltf_buffer_view> views (parsedData->buffer_views, parsedData->buffer_views_count);
    for(size_t i = 0; i < views.size(); i++)
    {
        const cgltf_buffer_view& view = views[i];
        tl::toStringBuffer(scratchStr, i);
        if(ImGui::CollapsingHeader(scratchStr))
        {
            ImGui::TreePush();
            static const char* typeStrs[] = {"invalid", "indices", "vertices"};
            ImGui::Text("Type: %s", typeStrs[view.type]);
            ImGui::Text("Offset: %ld bytes", view.offset);
            ImGui::Text("Size: %ld bytes", view.size);
            ImGui::Text("Stride: %ld bytes", view.stride);
            ImGui::Text("Buffer: %ld", getBufferInd(view.buffer));
            ImGui::TreePop();
        }
    }
}

void drawGui_accessorsTab()
{
    CArray<cgltf_accessor> accessors (parsedData->accessors, parsedData->accessors_count);
    for(size_t i = 0; i < accessors.size(); i++)
    {
        const cgltf_accessor& accessor = accessors[i];
        tl::toStringBuffer(scratchStr, i);
        if(ImGui::CollapsingHeader(scratchStr))
        {
            ImGui::TreePush();
            static const char* types[] = {"invalid", "scalar", "vec2", "vec3", "vec4", "mat2", "mat3", "mat4"};
            const char* typeStr = types[accessor.type];
            ImGui::Text("Type: %s", typeStr);
            static const char* componentTypeStrs[] = {"invalid", "i8", "u8", "i16", "u16", "32u", "32f"};
            const char* componentTypeStr = componentTypeStrs[accessor.component_type];
            ImGui::Text("Component type: %s", componentTypeStr);
            auto valStr = [type = accessor.type](const cgltf_float (&m)[16]) -> const char*
            {
                switch(type)
                {
                case cgltf_type_scalar:
                    sprintf(scratchStr, "%f", m[0]);
                    break;
                case cgltf_type_vec2:
                    sprintf(scratchStr, "{%f, %f}", m[0], m[1]);
                    break;
                case cgltf_type_vec3:
                    sprintf(scratchStr, "{%f, %f, %f}", m[0], m[1], m[2]);
                    break;
                case cgltf_type_vec4:
                    sprintf(scratchStr, "{%f, %f, %f, %f}", m[0], m[1], m[2], m[3]);
                    break;
                case cgltf_type_mat2:
                    sprintf(scratchStr, "{{%f, %f}, {%f, %f}}", m[0], m[1], m[2], m[3]);
                    break;
                case cgltf_type_mat3:
                    sprintf(scratchStr, "{{%f, %f, %f}, {%f, %f, %f}, {%f, %f, %f}}", m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
                    break;
                case cgltf_type_mat4:
                    sprintf(scratchStr, "{{%f, %f, %f, %f}, {%f, %f, %f, %f}, {%f, %f, %f, %f}, {%f, %f, %f, %f}}", m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15]);
                    break;
                default:
                    sprintf(scratchStr, "invalid");
                }
                return scratchStr;
            };
            if(accessor.has_min)
                ImGui::Text("Min: %s", valStr(accessor.min));
            if(accessor.has_max)
                ImGui::Text("Min: %s", valStr(accessor.max));

            ImGui::Text("Offset: %ld", accessor.offset);
            ImGui::Text("Count: %ld", accessor.count);
            ImGui::Text("Stride: %ld", accessor.stride);
            ImGui::Text("Normalized: %s", accessor.normalized ? "true" : "false");
            ImGui::Text("Buffer view: %ld", getBufferViewInd(accessor.buffer_view));

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

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(1,1));
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
        if(ImGui::BeginTabItem("Buffers")) {
            drawGui_buffersTab();
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("BufferViews")) {
            drawGui_bufferViewsTab();
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Accessors")) {
            drawGui_accessorsTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void loadTextures()
{
    CArray<cgltf_texture> textures(parsedData->textures, parsedData->textures_count);
    gpu::textures.resize(textures.size());
    glGenTextures(textures.size(), gpu::textures.begin());
    for(size_t i = 0; i < textures.size(); i++)
    {
        CStr mimeType = textures[i].image->mime_type;
        const auto* bufferView = textures[i].image->buffer_view;
        const auto* data = (u8*)bufferView->buffer->data + bufferView->offset;
        const size_t size = bufferView->size;
        int w, h, c;
        u8* img = stbi_load_from_memory(data, size, &w, &h, &c, 4);
        glBindTexture(GL_TEXTURE_2D, gpu::textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
        gpu::textureSizes[i] = {w, h};
        imgui_state::textureHeights[i] = DEFAULT_IMGUI_IMG_HEIGHT;
        const auto* sampler = textures[i].sampler;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sampler->min_filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, sampler->mag_filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sampler->wrap_s);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, sampler->wrap_t);
        if(sampler->min_filter == GL_NEAREST_MIPMAP_NEAREST ||
            sampler->min_filter == GL_LINEAR_MIPMAP_NEAREST ||
            sampler->min_filter == GL_NEAREST_MIPMAP_LINEAR ||
            sampler->min_filter == GL_LINEAR_MIPMAP_LINEAR
        ) glGenerateMipmap(GL_TEXTURE_2D);
    }
}

void loadBufferObjects()
{
    CArray<cgltf_buffer> buffers (parsedData->buffers, parsedData->buffers_count);
    gpu::bos.resize(buffers.size());
    glGenBuffers(gpu::bos.size(), gpu::bos.begin());
    for(size_t i = 0; i < buffers.size(); i++)
    {
        auto& buffer = buffers[i];
        glBindBuffer(GL_COPY_WRITE_BUFFER, gpu::bos[i]);
        glBufferData(GL_COPY_WRITE_BUFFER, buffer.size, buffer.data, GL_STATIC_DRAW);
    }
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
        loadTextures();
        loadBufferObjects();
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

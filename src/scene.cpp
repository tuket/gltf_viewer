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
constexpr size_t MAX_MATERIALS = 128;

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
struct MaterialTexturesHeights { float color; float metallicRoughness; };
static MaterialTexturesHeights materialTexturesHeights[MAX_MATERIALS];
}

void freeSceneGpuResources()
{
    using namespace gpu;
    glDeleteBuffers(bos.size(), bos.begin());
    glad_glDeleteVertexArrays(vaos.size(), vaos.begin());
    glDeleteTextures(textures.size(), textures.begin());
}

i32 selectedSceneInd = 0;

size_t getBufferInd(const cgltf_buffer* buffer) {
    return (size_t)(buffer - parsedData->buffers);
}
size_t getBufferViewInd(const cgltf_buffer_view* view) {
    return (size_t)(view - parsedData->buffer_views);
}
size_t getTextureInd(const cgltf_texture* tex) {
    return (size_t)(tex - parsedData->textures);
}
size_t getNodeInd(const cgltf_node* node) {
    return (size_t)(node - parsedData->nodes);
}
size_t getImageInd(const cgltf_image* image) {
    return (size_t)(image - parsedData->images);
}

void imguiTexture(size_t textureInd, float* height)
{
    const size_t i = textureInd;
    const float aspectRatio = (float)gpu::textureSizes[i].x / gpu::textureSizes->y;
    ImGui::SliderFloat("Scale", height, MIN_IMGUI_IMG_HEIGHT, gpu::textureSizes[i].y, "%.0f");
    ImGui::Image((void*)(u64)gpu::textures[i], {*height * aspectRatio, *height});
}

void imguiTextureView(const cgltf_texture_view& view, float* height)
{
    ImGui::Text("Scale: %f", view.scale);
    if(view.has_transform)
    if(ImGui::TreeNode("Transform")) {
        auto& tr = view.transform;
        ImGui::Text("Offset: {%f, %f}", tr.offset[0], tr.offset[1]);
        ImGui::Text("Scale: {%f, %f}", tr.scale[0], tr.scale[1]);
        ImGui::Text("Rotation: %f", tr.rotation);
        ImGui::Text("Texcoord index: %d", tr.texcoord);
    }
    ImGui::Text("Texcoord index: %d", view.texcoord);
    imguiTexture(getTextureInd(view.texture), height);
    cgltf_texture* texture;
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
    auto showSamplerData = [](const cgltf_sampler& sampler)
    {
        ImGui::Text("Min filter: %s", glMinFilterModeStr(sampler.min_filter));
        ImGui::Text("Mag filter: %s", glMinFilterModeStr(sampler.mag_filter));
        ImGui::Text("Wrap mode S: %s", glTextureWrapModeStr(sampler.wrap_s));
        ImGui::Text("Wrap mode T: %s", glTextureWrapModeStr(sampler.wrap_t));
    };
    auto showImage = [](const cgltf_image& image)
    {
        size_t i = getImageInd(&image);
        ImGui::Text("Name: %s", image.name);
        ImGui::Text("File path: %s", image.uri);
        ImGui::Text("MIME Type: %s", image.mime_type);
        ImGui::Text("Size: %dx%d", gpu::textureSizes[i].x, gpu::textureSizes[i].y);
        imguiTexture(i, &imgui_state::textureHeights[i]);
        // TODO
        /*if(ImGui::BeginPopupContextItem("right-click"))
        {
            if(ImGui::Selectable("copy")) {

            }
            ImGui::EndPopup();
        }*/
    };
    tl::CArray<cgltf_texture> textures(parsedData->textures, parsedData->textures_count);
    tl::toStringBuffer(scratchStr, "Textures (", textures.size(), ")");
    if(ImGui::CollapsingHeader(scratchStr))
    for(size_t i = 0; i < textures.size(); i++)
    {
        auto& texture = textures[i];
        tl::toStringBuffer(scratchStr, i, ") ", texture.name ? texture.name : "");
        if(ImGui::TreeNode((void*)&texture, "%s", scratchStr))
        {
            if(texture.image == nullptr) {
                ImGui::Text("image: (null)");
            }
            else if(ImGui::TreeNode((void*)&texture.image, "image")) {
                showImage(*texture.image);
                ImGui::TreePop();
            }
            if(texture.sampler == nullptr) {
                ImGui::Text("sampler: (null)");
            }
            else if(ImGui::TreeNode((void*)&texture.sampler, "sampler"))
            {
                showSamplerData(*texture.sampler);
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
    }

    tl::CArray<cgltf_image> images(parsedData->images, parsedData->images_count);
    tl::toStringBuffer(scratchStr, "Images (", images.size(), ")");
    if(ImGui::CollapsingHeader(scratchStr))
    for(size_t i = 0; i < images.size(); i++)
    if(ImGui::TreeNode((void*)&images[i], "%ld", i))
    {
        showImage(images[i]);
        ImGui::TreePop();
    }

    tl::CArray<cgltf_sampler> samplers(parsedData->samplers, parsedData->samplers_count);
    tl::toStringBuffer(scratchStr, "Samplers (", samplers.size(), ")");
    if(ImGui::CollapsingHeader(scratchStr))
    for(size_t i = 0; i < samplers.size(); i++)
    if(ImGui::TreeNode((void*)&samplers[i], "%ld", i))
    {
        showSamplerData(samplers[i]);
        ImGui::TreePop();
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

void drawGui_materialsTab()
{
    CArray<cgltf_material> materials(parsedData->materials, parsedData->materials_count);
    for(size_t i = 0; i < materials.size(); i++)
    {
        const cgltf_material& material = materials[i];
        tl::toStringBuffer(scratchStr, i, ") ", material.name ? material.name : "");
        if(ImGui::CollapsingHeader(scratchStr))
        {
            ImGui::TreePush();
            if(material.has_pbr_metallic_roughness)
            if(ImGui::TreeNode("PBR Metallic-Roughness"))
            {
                const auto& props = material.pbr_metallic_roughness;
                const auto& colorFactor = props.base_color_factor;
                ImGui::Text("Base color factor: {%f, %f, %f, %f}", colorFactor[0], colorFactor[1], colorFactor[2], colorFactor[3]);
                ImGui::Text("Metallic factor: %f", props.metallic_factor);
                ImGui::Text("Roughness factor: %f", props.roughness_factor);
                if(props.base_color_texture.texture)
                {
                    const size_t texInd = getTextureInd(props.base_color_texture.texture);
                    tl::toStringBuffer(scratchStr, "Color texture: ", i, " - ", gpu::textureSizes[texInd].x, "x", gpu::textureSizes[texInd].y);
                    if(ImGui::TreeNode(scratchStr)) {
                        imguiTextureView(props.base_color_texture, &imgui_state::materialTexturesHeights[i].color);
                        ImGui::TreePop();
                    }
                }
                if(props.metallic_roughness_texture.texture)
                {
                    const size_t texInd = getTextureInd(props.metallic_roughness_texture.texture);
                    tl::toStringBuffer(scratchStr, "Metallic-roughness texture: ", i, " - ", gpu::textureSizes[texInd].x, "x", gpu::textureSizes[texInd].y);
                    if(ImGui::TreeNode(scratchStr)) {
                        imguiTextureView(props.metallic_roughness_texture, &imgui_state::materialTexturesHeights[i].metallicRoughness);
                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
    }
}

void drawGui_skins()
{
    CArray<cgltf_skin> skins(parsedData->skins, parsedData->skins_count);
    for(size_t i = 0; i < skins.size(); i++)
    {
        auto& skin = skins[i];
        if(ImGui::TreeNode(&skin, "%ld) %s", i, skin.name ? skin.name : ""))
        {
            CArray<cgltf_node*> joints(skin.joints, skin.joints_count);
            if(ImGui::TreeNode((void*)skin.joints, "joints(%ld)", joints.size()))
            {
                for(size_t j = 0; j < joints.size(); j++)
                {
                    auto joint = joints[j];
                    if(joint == skin.skeleton)
                        ImGui::PushStyleColor(ImGuiCol_Text, 0xFF0000FF);
                    ImGui::Text("%ld) Node -> %ld) %s", j, getNodeInd(joint), (joint->name ? joint->name : ""));
                    if(joint == skin.skeleton)
                        ImGui::PopStyleColor();
                }
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
    }
}

void drawGui_samplers()
{
    CArray<cgltf_sampler> samplers (parsedData->samplers, parsedData->samplers_count);
    for(size_t i = 0; i < samplers.size(); i++)
    {
        tl::toStringBuffer(scratchStr, i);
        if(ImGui::CollapsingHeader(scratchStr))
        {
            ImGui::TreePush();
            const cgltf_sampler& sampler = samplers[i];
            ImGui::Text("Min filter: %s", glMinFilterModeStr(sampler.min_filter));
            ImGui::Text("Mag filter: %s", glMinFilterModeStr(sampler.mag_filter));
            ImGui::Text("Wrap mode S: %s", glTextureWrapModeStr(sampler.wrap_s));
            ImGui::Text("Wrap mode T: %s", glTextureWrapModeStr(sampler.wrap_t));
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
        if(ImGui::BeginTabItem("Materials")) {
            drawGui_materialsTab();
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Skins")) {
            drawGui_skins();
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

void loadMaterials()
{
    CArray<cgltf_material> materials (parsedData->materials, parsedData->materials_count);
    for(size_t i = 0; i < materials.size(); i++)
    {
        imgui_state::materialTexturesHeights[i] = {128.f, 128.f};
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
        loadMaterials();
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

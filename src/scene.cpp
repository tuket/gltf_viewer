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
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "utils.hpp"
#include "shaders.hpp"

using tl::FVector;
using tl::Array;
using tl::CArray;

static CStr openedFilePath = "";
static cgltf_data* parsedData = nullptr;
static cgltf_node* selectedNode = nullptr;
static i32 selectedCamera = -1; // -1 is the default orbit camera, indices >=0 are indices of the gltf camera
static OrbitCameraInfo orbitCam;

constexpr size_t MAX_BUFFER_OBJS = 256;
constexpr size_t MAX_VERT_ARRAYS = 128;
constexpr size_t MAX_TEXTURES = 128;
constexpr size_t MAX_MATERIALS = 128;
constexpr size_t MAX_TOTAL_PRIMITIVES = 1023;

// scene gpu resources
namespace gpu
{
static u32 metallicShader = 0;
static FVector<u32, MAX_BUFFER_OBJS> bos;
static FVector<u32, MAX_VERT_ARRAYS> vaos;
static FVector<u32, MAX_TOTAL_PRIMITIVES+1> meshPrimsVaos; // for mesh i, we can find here, at index i, the beginning of the vaos range, and at i+1 the end of that range
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

static void freeSceneGpuResources()
{
    using namespace gpu;
    glDeleteBuffers(bos.size(), bos.begin());
    glad_glDeleteVertexArrays(vaos.size(), vaos.begin());
    glDeleteTextures(textures.size(), textures.begin());
}

i32 selectedSceneInd = 0;

static size_t getBufferInd(const cgltf_buffer* buffer) {
    return (size_t)(buffer - parsedData->buffers);
}
static size_t getBufferViewInd(const cgltf_buffer_view* view) {
    return (size_t)(view - parsedData->buffer_views);
}
static size_t getAccessorInd(const cgltf_accessor* accessor) {
    return (size_t)(accessor - parsedData->accessors);
}
static size_t getTextureInd(const cgltf_texture* tex) {
    return (size_t)(tex - parsedData->textures);
}
static size_t getNodeInd(const cgltf_node* node) {
    return (size_t)(node - parsedData->nodes);
}
static size_t getImageInd(const cgltf_image* image) {
    return (size_t)(image - parsedData->images);
}
static size_t getMaterialInd(const cgltf_material* material) {
    return (size_t)(material - parsedData->materials);
}

static const char* getCameraName(int ind) {
    if(ind < 0)
        return "[DEFAULT ORBIT]";
    assert(ind < (int)parsedData->cameras_count);
    const char* name = parsedData->cameras[ind].name;
    tl::toStringBuffer(scratch.str, "%d) %s", (name ? name : "(null)"));
    return scratch.str;
}

static void imguiTexture(size_t textureInd, float* height)
{
    const size_t i = textureInd;
    const float aspectRatio = (float)gpu::textureSizes[i].x / gpu::textureSizes->y;
    ImGui::SliderFloat("Scale", height, MIN_IMGUI_IMG_HEIGHT, gpu::textureSizes[i].y, "%.0f");
    ImGui::Image((void*)(u64)gpu::textures[i], {*height * aspectRatio, *height});
}

static void imguiTextureView(const cgltf_texture_view& view, float* height)
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
}

static void imguiMaterial(const cgltf_material& material)
{
    const size_t i = getMaterialInd(&material);
    ImGui::Text("Name: %s", material.name);
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
            tl::toStringBuffer(scratch.str, "Color texture: ", i, " - ", gpu::textureSizes[texInd].x, "x", gpu::textureSizes[texInd].y);
            if(ImGui::TreeNode(scratch.str)) {
                imguiTextureView(props.base_color_texture, &imgui_state::materialTexturesHeights[i].color);
                ImGui::TreePop();
            }
        }
        if(props.metallic_roughness_texture.texture)
        {
            const size_t texInd = getTextureInd(props.metallic_roughness_texture.texture);
            tl::toStringBuffer(scratch.str, "Metallic-roughness texture: ", i, " - ", gpu::textureSizes[texInd].x, "x", gpu::textureSizes[texInd].y);
            if(ImGui::TreeNode(scratch.str)) {
                imguiTextureView(props.metallic_roughness_texture, &imgui_state::materialTexturesHeights[i].metallicRoughness);
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
}

static void imguiAccessor(const cgltf_accessor& accessor)
{
    ImGui::Text("Type: %s", cgltfTypeStr(accessor.type));
    ImGui::Text("Component type: %s", cgltfComponentTypeStr(accessor.component_type));
    if(accessor.has_min)
        ImGui::Text("Min: %s", cgltfValueStr(accessor.type, accessor.min));
    if(accessor.has_max)
        ImGui::Text("Min: %s", cgltfValueStr(accessor.type, accessor.max));

    ImGui::Text("Offset: %ld", accessor.offset);
    ImGui::Text("Count: %ld", accessor.count);
    ImGui::Text("Stride: %ld", accessor.stride);
    ImGui::Text("Normalized: %s", accessor.normalized ? "true" : "false");
    ImGui::Text("Buffer view: %ld", getBufferViewInd(accessor.buffer_view));
}

static void imguiAttribute(const cgltf_attribute& attrib)
{
    ImGui::Text("Index: %d", attrib.index);
    ImGui::Text("Name: %s", attrib.name);
    ImGui::Text("Type: %s", cgltfAttribTypeStr(attrib.type));
    if(ImGui::TreeNode((void*)&attrib.data, "Accessor %ld", getAccessorInd(attrib.data)))
    {
        imguiAccessor(*attrib.data);
        ImGui::TreePop();
    }
}

static void imguiPrimitive(const cgltf_primitive& prim)
{
    ImGui::Text("Type: %s", cgltfPrimitiveTypeStr(prim.type));
    if(prim.indices) {
        if(ImGui::TreeNode((void*)prim.indices,
                           "Indices (accessor %ld)", getAccessorInd(prim.indices)))
        {
            imguiAccessor(*prim.indices);
            ImGui::TreePop();
        }
    }

    if(prim.material == nullptr) {
        ImGui::Text("Material: NULL");
    }
    else {
        if (ImGui::TreeNode((void*)&prim.material, "Material (%ld)", getMaterialInd(prim.material))) {
            imguiMaterial(*prim.material);
            ImGui::TreePop();
        }
    }

    if(ImGui::TreeNode("Attributes")) {
        CArray<cgltf_attribute> attributes(prim.attributes, prim.attributes_count);
        for(size_t i = 0; i < attributes.size(); i++)
        {
            auto& attrib = attributes[i];
            if(ImGui::TreeNode((void*)&attrib, "%ld) %s", i, attrib.name))
            {
                imguiAttribute(attrib);
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }

    // THE FOLLOWNG CODE (Morph targets) HAS NOT BEEN TESTED
    if(prim.targets)
    if(ImGui::TreeNode("Morph targets")) {
        CArray<cgltf_morph_target> targets(prim.targets, prim.targets_count);
        for(size_t i = 0; i < targets.size(); i++)
        {
            auto& target = targets[i];
            if(target.attributes == nullptr) {
                ImGui::Text("Attributes: (null)");
            }
            else if(ImGui::TreeNode((void*)&target, "Attributes"))
            {
                CArray<cgltf_attribute> attribs(target.attributes, target.attributes_count);
                for(size_t i = 0; i < attribs.size(); i++)
                {
                    auto& attrib = attribs[i];
                    if(ImGui::TreeNode(&attrib, "%ld) %s", i, attrib.name))
                    {
                        imguiAttribute(attrib);
                        ImGui::TreePop();
                    }
                }
            }
        }
    }
}

static void drawSceneNodeRecursive(const cgltf_node* node, const glm::mat4& parentMat = glm::mat4(1.0))
{
    return; // DISABLED
    if(node->mesh)
    {
        glm::mat4 modelMat = parentMat * glm::make_mat4(node->matrix);
        CArray<cgltf_primitive> primitives(node->mesh->primitives, node->mesh->primitives_count);
        for(const cgltf_primitive& prim : primitives)
        {
            //ImGui::Text("Type: %s", cgltfTypeStr(prim.type));
            //prim.attributes->
        }
    }
}

void drawScene()
{
    glClearColor(0.1f, 0.2f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    return;
    if(!parsedData)
        return;
    CArray<cgltf_node*> nodes;
    for(cgltf_node* node : nodes)
        if(node->parent == nullptr)
            drawSceneNodeRecursive(node);
}

static void sceneNodeGuiRecusive(cgltf_node* node)
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

static void drawGui_scenesTab()
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
            tl::toStringBuffer(scratch.str, sceneInd, ") ", sceneName);
        }
        return scratch.str;
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

static void drawGui_meshesTab()
{
    const auto* meshes = parsedData->meshes;
    const size_t numMeshes = parsedData->meshes_count;
    for(size_t i = 0; i < numMeshes; i++)
    {
        auto& mesh = meshes[i];
        tl::toStringBuffer(scratch.str, i, ") ", mesh.name ? mesh.name : "");
        if(ImGui::CollapsingHeader(scratch.str))
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
                        imguiPrimitive(prim);
                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
    }
}

static void drawGui_texturesTab()
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
    tl::toStringBuffer(scratch.str, "Textures (", textures.size(), ")");
    if(ImGui::CollapsingHeader(scratch.str))
    for(size_t i = 0; i < textures.size(); i++)
    {
        auto& texture = textures[i];
        tl::toStringBuffer(scratch.str, i, ") ", texture.name ? texture.name : "");
        if(ImGui::TreeNode((void*)&texture, "%s", scratch.str))
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
    tl::toStringBuffer(scratch.str, "Images (", images.size(), ")");
    if(ImGui::CollapsingHeader(scratch.str))
    for(size_t i = 0; i < images.size(); i++)
    if(ImGui::TreeNode((void*)&images[i], "%ld", i))
    {
        showImage(images[i]);
        ImGui::TreePop();
    }

    tl::CArray<cgltf_sampler> samplers(parsedData->samplers, parsedData->samplers_count);
    tl::toStringBuffer(scratch.str, "Samplers (", samplers.size(), ")");
    if(ImGui::CollapsingHeader(scratch.str))
    for(size_t i = 0; i < samplers.size(); i++)
    if(ImGui::TreeNode((void*)&samplers[i], "%ld", i))
    {
        showSamplerData(samplers[i]);
        ImGui::TreePop();
    }
}

static void drawGui_buffersTab()
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
        tl::toStringBuffer(scratch.str, i, ") ", buffer.uri ? buffer.uri : "", sizeXBytes, unitsStrs[unitInd]);
        if(ImGui::CollapsingHeader(scratch.str))
        {
            // TODO
        }
    }
}

static void drawGui_bufferViewsTab()
{
    CArray<cgltf_buffer_view> views (parsedData->buffer_views, parsedData->buffer_views_count);
    for(size_t i = 0; i < views.size(); i++)
    {
        const cgltf_buffer_view& view = views[i];
        tl::toStringBuffer(scratch.str, i);
        if(ImGui::CollapsingHeader(scratch.str))
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

static void drawGui_accessorsTab()
{
    CArray<cgltf_accessor> accessors (parsedData->accessors, parsedData->accessors_count);
    for(size_t i = 0; i < accessors.size(); i++)
    {
        const cgltf_accessor& accessor = accessors[i];
        tl::toStringBuffer(scratch.str, i);
        if(ImGui::CollapsingHeader(scratch.str))
        {
            ImGui::TreePush();
            imguiAccessor(accessor);
            ImGui::TreePop();
        }
    }
}

static void drawGui_materialsTab()
{
    CArray<cgltf_material> materials(parsedData->materials, parsedData->materials_count);
    for(size_t i = 0; i < materials.size(); i++)
    {
        const cgltf_material& material = materials[i];
        tl::toStringBuffer(scratch.str, i, ") ", material.name ? material.name : "");
        if(ImGui::CollapsingHeader(scratch.str))
        {
            ImGui::TreePush();
            imguiMaterial(material);
            ImGui::TreePop();
        }
    }
}

static void drawGui_skins()
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

static void drawGui_samplers()
{
    CArray<cgltf_sampler> samplers (parsedData->samplers, parsedData->samplers_count);
    for(size_t i = 0; i < samplers.size(); i++)
    {
        tl::toStringBuffer(scratch.str, i);
        if(ImGui::CollapsingHeader(scratch.str))
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

static void drawGui_cameras()
{
    CArray<cgltf_camera> cameras(parsedData->cameras, parsedData->cameras_count);

    if(ImGui::BeginCombo("Use camera", getCameraName(selectedCamera)))
    {
        for(i64 i = -1; i < (i64)cameras.size(); i++)
        {
            ImGui::PushID((void*)i);
            if (ImGui::Selectable(getCameraName(i)))
                selectedSceneInd = i;
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    if(ImGui::CollapsingHeader("Cameras")) {
        for(size_t i = 0; i < cameras.size(); i++)
        {
            auto& camera = cameras[i];
            if(ImGui::TreeNode((void*)&camera, "%ld) %s", i, camera.name))
            {
                ImGui::Text("Name: %s", camera.name);
                ImGui::Text("Type: %s", cgltfCameraTypeStr(camera.type));
                if(camera.type == cgltf_camera_type_perspective) {
                    auto& data = camera.data.perspective;
                    ImGui::Text("Aspect ratio: %f", data.aspect_ratio);
                    ImGui::Text("Fov Y: %f", data.yfov);
                    ImGui::Text("Near: %f", data.znear);
                    ImGui::Text("Far: %f", data.zfar);
                }
                else if(camera.type == cgltf_camera_type_orthographic) {
                    auto& data = camera.data.orthographic;
                    ImGui::Text("SizeXY: %f x %f", data.xmag, data.ymag);
                    ImGui::Text("Near: %f", data.znear);
                    ImGui::Text("Far: %f", data.zfar);
                }
                ImGui::TreePop();
            }
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
    tl::toStringBuffer(scratch.str, openedFilePath,"##0");
    ImGui::Begin(scratch.str);
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
        if(ImGui::BeginTabItem("Cameras")) {
            drawGui_cameras();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

static void loadTextures()
{
    CArray<cgltf_texture> textures(parsedData->textures, parsedData->textures_count);
    gpu::textures.resize(textures.size());
    glGenTextures(textures.size(), gpu::textures.begin());
    for(size_t i = 0; i < textures.size(); i++)
    {
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

static void loadBufferObjects()
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

static void createVaos()
{
    CArray<cgltf_mesh> meshes (parsedData->meshes, parsedData->meshes_count);
    gpu::meshPrimsVaos.resize(meshes.size()+1);
    gpu::meshPrimsVaos[0] = 0;
    u32 rangeInd = 0;
    for(size_t meshInd = 0; meshInd < meshes.size(); meshInd++)
    {
        auto& mesh = meshes[meshInd];
        CArray<cgltf_primitive> prims(mesh.primitives, mesh.primitives_count);
        rangeInd += prims.size();
        gpu::meshPrimsVaos[meshInd+1] = rangeInd;
    }
    gpu::vaos.resize(rangeInd);
    glGenVertexArrays(rangeInd, gpu::vaos.begin());

    for(size_t meshInd = 0; meshInd < meshes.size(); meshInd++)
    {
        auto& mesh = meshes[meshInd];
        const u32 vaoBeginInd = gpu::meshPrimsVaos[meshInd];
        CArray<cgltf_primitive> prims(mesh.primitives, mesh.primitives_count);
        for(size_t primInd = 0; primInd < prims.size(); primInd++)
        {
            auto& prim = prims[primInd];
            const u32 vao = gpu::vaos[vaoBeginInd + primInd];
            glBindVertexArray(vao);
            CArray<cgltf_attribute> attribs(prim.attributes, prim.attributes_count);
            u32 availableAttribsMask = 0;
            for(cgltf_attribute attrib : attribs) {
                EAttrib eAttrib = strToEAttrib(attrib.name);
                assert(eAttrib != EAttrib::COUNT);
                const u32 attribInd = (u32)eAttrib;
                availableAttribsMask |= 1 << attribInd;
                glEnableVertexAttribArray(attribInd);
                const cgltf_accessor* accessor = attrib.data;
                const GLint numComponents = cgltfTypeNumComponents(accessor->type);
                const GLenum componentType = cgltfComponentTypeToGl(accessor->component_type);
                glVertexAttribPointer(attribInd, numComponents, componentType, (u8)accessor->normalized, accessor->stride, (void*)accessor->offset);
            }
            if(availableAttribsMask & ((u32)EAttrib::NORMAL | (u32)EAttrib::TANGENT))
            {
                availableAttribsMask |= (u32)EAttrib::COTANGENT;
                // TODO
            }
            auto checkAvailable = [availableAttribsMask](EAttrib eAttrib) { return (u32)eAttrib & availableAttribsMask; };
            if(!)
                assert(checkAvailable(EAttrib::POSITION) && checkAvailable(EAttrib::NORMAL));
            if(!)
        }
    }
}

static void loadMaterials()
{
    CArray<cgltf_material> materials (parsedData->materials, parsedData->materials_count);
    for(size_t i = 0; i < materials.size(); i++)
    {
        imgui_state::materialTexturesHeights[i] = {128.f, 128.f};
    }
}

static void setupOrbitCamera()
{
    // we should compute here nice values so we can view the whole scene
    orbitCam.pitch = 0;
    orbitCam.heading = 0;
    orbitCam.distance = 100;
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
        crateVaos();
        loadMaterials();
        setupOrbitCamera();
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

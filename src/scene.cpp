#include "scene.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
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
#include <tg/cameras.hpp>

using tl::FVector;
using tl::Span;
using tl::CSpan;

extern GLFWwindow* window;

static Str openedFilePath = "";
static cgltf_data* parsedData = nullptr;
static cgltf_node* selectedNode = nullptr;
static i32 selectedCamera = -1; // -1 is the default orbit camera, indices >=0 are indices of the gltf camera
static struct OrbitCameraInfo{ float heading, pitch, distance; } orbitCam;
static const CameraProjectionInfo camProjInfo = {glm::radians(45.f), 0.02f, 500.f};

static glm::mat4 camProjMtx;

constexpr size_t MAX_BUFFER_OBJS = 256;
constexpr size_t MAX_VERT_ARRAYS = 128;
constexpr size_t MAX_TEXTURES = 128;
constexpr size_t MAX_MATERIALS = 128;
constexpr size_t MAX_TOTAL_PRIMITIVES = 1023;

// scene gpu resources
namespace gpu
{
static u32 basicSampler;
static u32 whiteTexture;
static u32 blueTexture;
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
static u64 lightEnableBits = -1;
static i32 selectedSceneInd = 0;
}

namespace mouse_handling
{
    static bool pressed = false;
    static float prevX, prevY;
    void onMouseButton(GLFWwindow* window, int button, int action, int mods)
    {
        if(ImGui::GetIO().WantCaptureMouse)
            return; // the mouse is captured by imgui
        if(button == GLFW_MOUSE_BUTTON_LEFT)
            pressed = action == GLFW_PRESS;
    }
    void onMouseMove(GLFWwindow* window, double x, double y)
    {
        if(selectedCamera == -1) // orbit camera is active
        if(pressed)
        {
            const float dx = (float)x - prevX;
            const float dy = (float)y - prevY;
            constexpr float speed = PI;
            int windowW, windowH;
            glfwGetWindowSize(window, &windowW, &windowH);
            orbitCam.heading += speed * dx / windowW;
            while(orbitCam.heading < 0)
                orbitCam.heading += 2*PI;
            while(orbitCam.heading > 2*PI)
                orbitCam.heading -= 2*PI;
            orbitCam.pitch += speed * dy / windowH;
            orbitCam.pitch = glm::clamp(orbitCam.pitch, -0.45f*PI, +0.45f*PI);
        }
        prevX = (float)x;
        prevY = (float)y;
    }
    void onMouseWheel(GLFWwindow* window, double dx, double dy)
    {
        if(ImGui::GetIO().WantCaptureMouse)
            return; // the mouse is captured by imgui
        const float speed = 1.04f;
        orbitCam.distance *= pow(speed, (float)dy);
        orbitCam.distance = glm::max(orbitCam.distance, 0.01f);
    }
}

static void freeSceneGpuResources()
{
    using namespace gpu;
    glDeleteBuffers(bos.size(), bos.begin());
    glDeleteVertexArrays(vaos.size(), vaos.begin());
    glDeleteTextures(textures.size(), textures.begin());
}

void createBasicTextures()
{
    glGenSamplers(1, &gpu::basicSampler);
    glSamplerParameteri(gpu::basicSampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(gpu::basicSampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenTextures(2, &gpu::whiteTexture);

    const u8 white[3] = {255, 255, 255};
    glBindTexture(GL_TEXTURE_2D, gpu::whiteTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, white);

    const u8 blue[3] = {0, 0, 255};
    glBindTexture(GL_TEXTURE_2D, gpu::blueTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, blue);
}


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
static size_t getMeshInd(const cgltf_mesh* mesh) {
    return (size_t)(mesh - parsedData->meshes);
}
static size_t getSkinInd(const cgltf_skin* skin) {
    return (size_t)(skin - parsedData->skins);
}
static size_t getCameraInd(const cgltf_camera* camera) {
    return (size_t)(camera - parsedData->cameras);
}
static size_t getLightInd(const cgltf_light* light) {
    return (size_t)(light - parsedData->lights);
}

static const char* getCameraName(int ind) {
    if(ind < 0)
        return "[DEFAULT ORBIT]";
    assert(ind < (int)parsedData->cameras_count);
    const char* name = parsedData->cameras[ind].name;
    tl::toStringBuffer(scratchStr(), "%d) %s", (name ? name : "(null)"));
    return scratchStr();
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
    ImGui::Text("Scale: %g", view.scale);
    if(view.has_transform)
    if(ImGui::TreeNode("Transform")) {
        auto& tr = view.transform;
        ImGui::Text("Offset: {%g, %g}", tr.offset[0], tr.offset[1]);
        ImGui::Text("Scale: {%g, %g}", tr.scale[0], tr.scale[1]);
        ImGui::Text("Rotation: %g", tr.rotation);
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
        ImGui::Text("Base color factor: {%g, %g, %g, %g}", colorFactor[0], colorFactor[1], colorFactor[2], colorFactor[3]);
        ImGui::Text("Metallic factor: %g", props.metallic_factor);
        ImGui::Text("Roughness factor: %g", props.roughness_factor);
        if(props.base_color_texture.texture)
        {
            const size_t texInd = getTextureInd(props.base_color_texture.texture);
            tl::toStringBuffer(scratchStr(), "Color texture: ", i, " - ", gpu::textureSizes[texInd].x, "x", gpu::textureSizes[texInd].y);
            if(ImGui::TreeNode(scratchStr())) {
                imguiTextureView(props.base_color_texture, &imgui_state::materialTexturesHeights[i].color);
                ImGui::TreePop();
            }
        }
        if(props.metallic_roughness_texture.texture)
        {
            const size_t texInd = getTextureInd(props.metallic_roughness_texture.texture);
            tl::toStringBuffer(scratchStr(), "Metallic-roughness texture: ", i, " - ", gpu::textureSizes[texInd].x, "x", gpu::textureSizes[texInd].y);
            if(ImGui::TreeNode(scratchStr())) {
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
        CSpan<cgltf_attribute> attributes(prim.attributes, prim.attributes_count);
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
        CSpan<cgltf_morph_target> targets(prim.targets, prim.targets_count);
        for(size_t i = 0; i < targets.size(); i++)
        {
            auto& target = targets[i];
            if(target.attributes == nullptr) {
                ImGui::Text("Attributes: (null)");
            }
            else if(ImGui::TreeNode((void*)&target, "Attributes"))
            {
                CSpan<cgltf_attribute> attribs(target.attributes, target.attributes_count);
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

static void imguiColor(const float (&color) [3])
{
    ImGui::TextColored(ImColor(color[0], color[1], color[2]), "(%g, %g, %g)", color[0], color[1], color[2]);
}

static void imguiLight(const cgltf_light& light)
{
    static const char* lightTypeStrs[] = {
        "invalid",
        "directional",
        "point",
        "spot"
    };
    ImGui::Text("Name: %s", light.name);
    ImGui::Text("Color: ");
    ImGui::SameLine();
    imguiColor(light.color);
    ImGui::Text("Intensity: %g", light.intensity);
    ImGui::Text("Range: %g", light.range);
    assert((size_t)light.type < tl::size(lightTypeStrs));
    ImGui::Text("Type: %s", lightTypeStrs[light.type]);
    if(light.type == cgltf_light_type_spot) {
        ImGui::Text("Inner cone angle: %g", light.spot_inner_cone_angle);
        ImGui::Text("Outer cone angle: %g", light.spot_outer_cone_angle);
    }
}

static const cgltf_material s_defaultMaterial =
{
    nullptr, // name
    true, // has_pbr_metallic_roughness
    false, //cgltf_bool has_pbr_specular_glossiness
    cgltf_pbr_metallic_roughness {
        cgltf_texture_view { // base_color_texture

        },
        cgltf_texture_view { // metallic_roughness_texture

        },
        {1.f, 1.f, 1.f, 1.f}, // base_color_factor
        1.f, // metallic_factor;
        1.f, // roughness_factor;
    },
    cgltf_pbr_specular_glossiness {},
    cgltf_texture_view {nullptr}, // normal_texture
    cgltf_texture_view {nullptr}, // occlusion_texture
    cgltf_texture_view {nullptr}, // emissive_texture;
    {0.f, 0.f, 0.f}, // emissive_factor
    cgltf_alpha_mode_opaque,
    0.5f, // cgltf_float alpha_cutoff
    false, // double_sided
    false, // unlit
};

static void drawSceneNodeRecursive(const cgltf_node& node, const glm::mat4& parentMat = glm::mat4(1.0))
{
    //return; // DISABLED
    const glm::mat4 modelMat = parentMat * glm::make_mat4(node.matrix);
    if(node.mesh)
    {
        const glm::mat4 viewMat = tg::calcOrbitCameraMtx(orbitCam.heading, orbitCam.pitch, orbitCam.distance);
        const glm::mat4 modelViewMat = viewMat * modelMat;
        const glm::mat3 modelMat3 = modelMat;
        const glm::mat4 modelViewProj = camProjMtx * modelViewMat;
        CSpan<cgltf_primitive> primitives(node.mesh->primitives, node.mesh->primitives_count);
        const u32* vaos = gpu::vaos.begin() + gpu::meshPrimsVaos[getMeshInd(node.mesh)];
        for(size_t i = 0; i < primitives.size(); i++)
        {
            const cgltf_primitive& prim = primitives[i];
            const u32 vao = vaos[i];
            const auto& material = prim.material ? *prim.material : s_defaultMaterial;

            auto draw = [&]
            {
                glActiveTexture(GL_TEXTURE0 + (u32)ETexUnit::ALBEDO);
                if(material.has_pbr_metallic_roughness) {
                    glUniform4fv(gpu::shaderPbrMetallic().unifLocs.color,
                                 1, material.pbr_metallic_roughness.base_color_factor);
                    if(auto tex = material.pbr_metallic_roughness.base_color_texture.texture)
                        ;
                    else
                        glBindTexture(GL_TEXTURE_2D, gpu::whiteTexture);
                }
                else if(material.has_pbr_specular_glossiness) {
                    assert("todo" && false);
                }

                glActiveTexture(GL_TEXTURE0 + (u32)ETexUnit::NORMAL);
                if(auto tex = material.normal_texture.texture)
                    ;
                else
                    glBindTexture(GL_TEXTURE_2D, gpu::blueTexture);

                glBindVertexArray(vao);
                if(prim.indices) {
                    glDrawElements(
                       cgltfPrimTypeToGl(prim.type),
                       prim.indices->count,
                       cgltfComponentTypeToGl(prim.indices->component_type),
                       (void*)prim.indices->buffer_view->offset
                    );
                }
                else {
                    glDrawArrays(cgltfPrimTypeToGl(prim.type), 0, prim.attributes->data->count);
                }
            };
            auto uploadCommonUniforms = [&](const ShaderData& shader)
            {
                glUniformMatrix4fv(shader.unifLocs.modelViewProj, 1, GL_FALSE, &modelViewProj[0][0]);
                glUniformMatrix3fv(shader.unifLocs.modelMat3, 1, GL_FALSE, &modelMat3[0][0]);
            };

            if(material.has_pbr_metallic_roughness)
            {
                auto& shader = gpu::shaderPbrMetallic();
                glUseProgram(shader.prog);
                uploadCommonUniforms(shader);
                draw();
            }
            else {
                assert(false && "type of material not supported");
            }
        }
    }

    CSpan<cgltf_node*> children(node.children, node.children_count);
    for(cgltf_node* child : children) {
        assert(child);
        drawSceneNodeRecursive(*child, modelMat);
    }
}

void drawScene()
{
    glClearColor(0.1f, 0.2f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if(!parsedData)
        return;
    //return;
    int w, h;
    glfwGetWindowSize(window, &w, &h);
    camProjMtx = glm::perspective(camProjInfo.fovY, (float)w / h, camProjInfo.nearDist, camProjInfo.farDist);
    CSpan<cgltf_node> nodes(parsedData->nodes, parsedData->nodes_count);
    for(const cgltf_node& node : nodes)
        if(node.parent == nullptr)
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
    auto sceneComboDisplayStr = [](i32 sceneInd) -> const char* {
        const char* sceneName = "";
        if(sceneInd != -1) {
            sceneName = parsedData->scenes[sceneInd].name;
            sceneName = sceneName ? sceneName : "";
            tl::toStringBuffer(scratchStr(), sceneInd, ") ", sceneName);
        }
        return scratchStr();
    };
    if(ImGui::BeginCombo("scene", sceneComboDisplayStr(imgui_state::selectedSceneInd)))
    {
        for(i32 sceneInd = 0; sceneInd < (i32)parsedData->scenes_count; sceneInd++)
        {
            const auto& scene = parsedData->scenes[sceneInd];
            ImGui::PushID((void*)&scene);
            if (ImGui::Selectable(sceneComboDisplayStr(sceneInd)))
                imgui_state::selectedSceneInd = sceneInd;
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    if(imgui_state::selectedSceneInd != -1)
    {
        const cgltf_scene& selectedScene = parsedData->scenes[imgui_state::selectedSceneInd];
        for(size_t nodeInd = 0; nodeInd < selectedScene.nodes_count; nodeInd++)
            if(selectedScene.nodes[nodeInd]->parent == nullptr)
                sceneNodeGuiRecusive(selectedScene.nodes[nodeInd]);
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("right_panel", {rightPanelSize, -1}, true);
    if(selectedNode) {
        if(selectedNode->skin) {
            ImGui::Text("Skin: %ld", getSkinInd(selectedNode->skin));
        }
        if(selectedNode->mesh) {
            ImGui::Text("Mesh: %ld", getMeshInd(selectedNode->mesh));
        }
        if(selectedNode->camera) {
            ImGui::Text("Camera: %ld", getCameraInd(selectedNode->camera));
        }
        if(selectedNode->light) {
            ImGui::Text("Light: %ld", getLightInd(selectedNode->light));
        }
        if(selectedNode->weights) {
            // NOT TESTED
            if(ImGui::TreeNode(&selectedNode->weights, "Weights(%ld)", selectedNode->weights_count)) {
                for(size_t i = 0; i < selectedNode->weights_count; i++)
                    ImGui::Text("%g", selectedNode->weights[i]);
                ImGui::TreePop();
            }
        }
        if(selectedNode->has_translation) {
            const float* p = selectedNode->translation;
            ImGui::Text("Translation: {%g, %g, %g}", p[0], p[1], p[2]);
        }
        if(selectedNode->has_rotation) {
            const float* q = selectedNode->rotation;
            ImGui::Text("Rotation: {%g, %g, %g, %g}", q[0], q[1], q[2], q[3]);
        }
        if(selectedNode->has_scale) {
            const float* s = selectedNode->scale;
            ImGui::Text("Scale: {%g, %g, %g}", s[0], s[1], s[2]);
        }
        if(selectedNode->has_matrix) {
            const float* m = selectedNode->matrix;
            ImGui::Text("Matrix: {\n"
                        "  %g, %g, %g, %g,\n"
                        "  %g, %g, %g, %g,\n"
                        "  %g, %g, %g, %g,\n"
                        "  %g, %g, %g, %g}",
                        m[0], m[1], m[2], m[3],
                        m[4], m[4], m[6], m[7],
                        m[8], m[9], m[10], m[11],
                        m[12], m[13], m[14], m[15]);
        }
        ImGui::Text("be");
    }
    else {
        ImGui::Text("Nothing selected");
    }
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
        tl::toStringBuffer(scratchStr(), i, ") ", mesh.name ? mesh.name : "");
        if(ImGui::CollapsingHeader(scratchStr()))
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
    tl::CSpan<cgltf_texture> textures(parsedData->textures, parsedData->textures_count);
    tl::toStringBuffer(scratchStr(), "Textures (", textures.size(), ")");
    if(ImGui::CollapsingHeader(scratchStr()))
    for(size_t i = 0; i < textures.size(); i++)
    {
        auto& texture = textures[i];
        tl::toStringBuffer(scratchStr(), i, ") ", texture.name ? texture.name : "");
        if(ImGui::TreeNode((void*)&texture, "%s", scratch.ptr<char>()))
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

    tl::CSpan<cgltf_image> images(parsedData->images, parsedData->images_count);
    tl::toStringBuffer(scratchStr(), "Images (", images.size(), ")");
    if(ImGui::CollapsingHeader(scratchStr()))
    for(size_t i = 0; i < images.size(); i++)
    if(ImGui::TreeNode((void*)&images[i], "%ld", i))
    {
        showImage(images[i]);
        ImGui::TreePop();
    }

    tl::CSpan<cgltf_sampler> samplers(parsedData->samplers, parsedData->samplers_count);
    tl::toStringBuffer(scratchStr(), "Samplers (", samplers.size(), ")");
    if(ImGui::CollapsingHeader(scratchStr()))
    for(size_t i = 0; i < samplers.size(); i++)
    if(ImGui::TreeNode((void*)&samplers[i], "%ld", i))
    {
        showSamplerData(samplers[i]);
        ImGui::TreePop();
    }
}

static void drawGui_buffersTab()
{
    CSpan<cgltf_buffer> buffers (parsedData->buffers, parsedData->buffers_count);
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
        tl::toStringBuffer(scratchStr(), i, ") ", buffer.uri ? buffer.uri : "", sizeXBytes, unitsStrs[unitInd]);
        if(ImGui::CollapsingHeader(scratchStr()))
        {
            // TODO
        }
    }
}

static void drawGui_bufferViewsTab()
{
    CSpan<cgltf_buffer_view> views (parsedData->buffer_views, parsedData->buffer_views_count);
    for(size_t i = 0; i < views.size(); i++)
    {
        const cgltf_buffer_view& view = views[i];
        tl::toStringBuffer(scratchStr(), i);
        if(ImGui::CollapsingHeader(scratchStr()))
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
    CSpan<cgltf_accessor> accessors (parsedData->accessors, parsedData->accessors_count);
    for(size_t i = 0; i < accessors.size(); i++)
    {
        const cgltf_accessor& accessor = accessors[i];
        tl::toStringBuffer(scratchStr(), i);
        if(ImGui::CollapsingHeader(scratchStr()))
        {
            ImGui::TreePush();
            imguiAccessor(accessor);
            ImGui::TreePop();
        }
    }
}

static void drawGui_materialsTab()
{
    CSpan<cgltf_material> materials(parsedData->materials, parsedData->materials_count);
    for(size_t i = 0; i < materials.size(); i++)
    {
        const cgltf_material& material = materials[i];
        tl::toStringBuffer(scratchStr(), i, ") ", material.name ? material.name : "");
        if(ImGui::CollapsingHeader(scratchStr()))
        {
            ImGui::TreePush();
            imguiMaterial(material);
            ImGui::TreePop();
        }
    }
}

static void drawGui_skins()
{
    CSpan<cgltf_skin> skins(parsedData->skins, parsedData->skins_count);
    for(size_t i = 0; i < skins.size(); i++)
    {
        auto& skin = skins[i];
        if(ImGui::TreeNode(&skin, "%ld) %s", i, skin.name ? skin.name : ""))
        {
            CSpan<cgltf_node*> joints(skin.joints, skin.joints_count);
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
    CSpan<cgltf_sampler> samplers (parsedData->samplers, parsedData->samplers_count);
    for(size_t i = 0; i < samplers.size(); i++)
    {
        tl::toStringBuffer(scratchStr(), i);
        if(ImGui::CollapsingHeader(scratchStr()))
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
    CSpan<cgltf_camera> cameras(parsedData->cameras, parsedData->cameras_count);

    if(ImGui::BeginCombo("Use camera", getCameraName(selectedCamera)))
    {
        for(i64 i = -1; i < (i64)cameras.size(); i++)
        {
            ImGui::PushID((void*)i);
            if (ImGui::Selectable(getCameraName(i)))
                imgui_state::selectedSceneInd = i;
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
                    ImGui::Text("Aspect ratio: %g", data.aspect_ratio);
                    ImGui::Text("Fov Y: %g", data.yfov);
                    ImGui::Text("Near: %g", data.znear);
                    ImGui::Text("Far: %g", data.zfar);
                }
                else if(camera.type == cgltf_camera_type_orthographic) {
                    auto& data = camera.data.orthographic;
                    ImGui::Text("SizeXY: %g x %g", data.xmag, data.ymag);
                    ImGui::Text("Near: %g", data.znear);
                    ImGui::Text("Far: %g", data.zfar);
                }
                ImGui::TreePop();
            }
        }
    }
}

static void drawGui_lights()
{
    CSpan<cgltf_light> lights(parsedData->lights, parsedData->lights_count);
    for(size_t i = 0; i < lights.size(); i++)
    {
        const cgltf_light& light = lights[i];
        if(ImGui::TreeNode((void*)&parsedData->lights, "%ld) %s", i, light.name)) {
            imguiLight(light);
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
    tl::toStringBuffer(scratchStr(), openedFilePath, "##0");
    ImGui::Begin(scratchStr());
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
        if(ImGui::BeginTabItem("Lights")) {
            drawGui_lights();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

static void loadTextures()
{
    CSpan<cgltf_texture> textures(parsedData->textures, parsedData->textures_count);
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
        if(sampler) {
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
        else { // there is no sampler, glTF specs some defaults when there is no sampler
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        
    }
}

static void loadBufferObjects()
{
    CSpan<cgltf_buffer> buffers (parsedData->buffers, parsedData->buffers_count);
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
    CSpan<cgltf_mesh> meshes (parsedData->meshes, parsedData->meshes_count);
    gpu::meshPrimsVaos.resize(meshes.size()+1);
    gpu::meshPrimsVaos[0] = 0;
    u32 rangeInd = 0;
    for(size_t meshInd = 0; meshInd < meshes.size(); meshInd++)
    {
        auto& mesh = meshes[meshInd];
        CSpan<cgltf_primitive> prims(mesh.primitives, mesh.primitives_count);
        rangeInd += prims.size();
        gpu::meshPrimsVaos[meshInd+1] = rangeInd;
    }
    gpu::vaos.resize(rangeInd);
    glGenVertexArrays(rangeInd, gpu::vaos.begin());

    for(size_t meshInd = 0; meshInd < meshes.size(); meshInd++)
    {
        auto& mesh = meshes[meshInd];
        const u32 vaoBeginInd = gpu::meshPrimsVaos[meshInd];
        CSpan<cgltf_primitive> prims(mesh.primitives, mesh.primitives_count);
        for(size_t primInd = 0; primInd < prims.size(); primInd++)
        {
            auto& prim = prims[primInd];
            const u32 vao = gpu::vaos[vaoBeginInd + primInd];
            glBindVertexArray(vao);
            if(prim.indices) {
                const u32 ebo = gpu::bos[getBufferInd(prim.indices->buffer_view->buffer)];
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
            }
            CSpan<cgltf_attribute> attribs(prim.attributes, prim.attributes_count);
            u32 availableAttribsMask = 0;
            u8 attribIdToInd[(int)EAttrib::COUNT];
            for(size_t attribInd = 0; attribInd < attribs.size(); attribInd++)
            {
                const cgltf_attribute& attrib = attribs[attribInd];
                EAttrib eAttrib = strToEAttrib(attrib.name);
                assert(eAttrib != EAttrib::COUNT);
                const u32 attribId = (u32)eAttrib;
                availableAttribsMask |= 1 << attribId;
                glEnableVertexAttribArray(attribId);
                const cgltf_accessor* accessor = attrib.data;
                const GLint numComponents = cgltfTypeNumComponents(accessor->type);
                const GLenum componentType = cgltfComponentTypeToGl(accessor->component_type);
                const size_t offset = accessor->offset + accessor->buffer_view->offset;
                const u32 vbo = gpu::bos[getBufferInd(accessor->buffer_view->buffer)];
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                glVertexAttribPointer(attribId, numComponents, componentType,
                    (u8)accessor->normalized, accessor->stride, (void*)offset);
                attribIdToInd[attribId] = (u8)attribInd;
            }
            assert((availableAttribsMask & (1U << (u32)EAttrib::POSITION)) &&
                   (availableAttribsMask & (1U << (u32)EAttrib::NORMAL)));

            { // setup tangents and cotangents buffer
                const cgltf_accessor& normalAttribData = *attribs[attribIdToInd[(u32)EAttrib::NORMAL]].data;
                assert(normalAttribData.type == cgltf_type_vec3 && normalAttribData.component_type == cgltf_component_type_r_32f);
                const size_t numVerts = normalAttribData.count;
                const bool gottaGenerateTangets = (availableAttribsMask & (1U << (u32)EAttrib::TANGENT)) == 0;
                const size_t tangentsNumBytes = sizeof(glm::vec3) * numVerts;
                const size_t bufSize = tangentsNumBytes * (gottaGenerateTangets ? 2 : 1);
                scratch.growIfNeeded(bufSize);
                u32 vbo;
                glGenBuffers(1, &vbo);
                gpu::bos.push_back(vbo);
                glBindBuffer(GL_ARRAY_BUFFER, vbo);
                auto normals = (const u8*)normalAttribData.buffer_view->buffer->data + normalAttribData.offset;
                const size_t normalsStride = normalAttribData.stride ? normalAttribData.stride : sizeof(glm::vec3);
                glm::vec3* tangents;
                if(gottaGenerateTangets)
                {
                    // generate tangents, if there aren't any
                    auto normalPtr = normals;
                    tangents = scratch.ptr<glm::vec3>();
                    for(size_t i = 0; i < numVerts; i++) {
                        const glm::vec3& n = *reinterpret_cast<const glm::vec3*>(normalPtr);
                        // find some vector perpendicular to n
                        glm::vec3 x {1, 0, 0};
                        if(abs(dot(x, n)) > 0.99f)
                            x = {0, 1, 0};
                        tangents[i] = cross(n, x);
                        normalPtr += normalsStride;
                    }
                    glEnableVertexAttribArray((u32)EAttrib::TANGENT);
                    glVertexAttribPointer((u32)EAttrib::TANGENT, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
                }
                else {
                    const cgltf_accessor& tangentAttribData = *attribs[attribIdToInd[(u32)EAttrib::TANGENT]].data;
                    tangents = (glm::vec3*)tangentAttribData.buffer_view->buffer->data;
                    assert(numVerts == tangentAttribData.count);
                    assert(tangentAttribData.type == cgltf_type_vec3 && tangentAttribData.component_type == cgltf_component_type_r_32f);
                }

                { // generate cotangents
                    auto normalPtr = normals;
                    glm::vec3* cotangents = scratch.ptr<glm::vec3>() + (gottaGenerateTangets ? numVerts : 0);
                    for(size_t i = 0; i < numVerts; i++) {
                        const glm::vec3 n = *reinterpret_cast<const glm::vec3*>(normalPtr);
                        cotangents[i] = cross(n, tangents[i]);
                        normalPtr += normalsStride;
                    }

                    glEnableVertexAttribArray((u32)EAttrib::COTANGENT);
                    glVertexAttribPointer((u32)EAttrib::COTANGENT, 3, GL_FLOAT, GL_FALSE,
                        0, (void*)(gottaGenerateTangets ? tangentsNumBytes : 0));
                }

                glBufferData(GL_ARRAY_BUFFER, bufSize, scratch.ptr<u8>(), GL_STATIC_DRAW);
            }

            if(availableAttribsMask & (u32)EAttrib::COLOR) {
                const u32 attribInd = (u32) EAttrib::COLOR;
                //glDisableVertexAttribArray() // attribs shuld be disabled by default
                glVertexAttrib4f(attribInd, 1, 1, 1, 1); // here we set a default value (white) for the color vertex (it would be 0,0,0,1 otherwise)
            }
        }
    }
}

static void loadMaterials()
{
    CSpan<cgltf_material> materials (parsedData->materials, parsedData->materials_count);
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
    orbitCam.distance = 3.f;
}

bool loadGltf(const char* path)
{
    openedFilePath = path;
    cgltf_data* newParsedData = parsedData;
    cgltf_options options = {};
    cgltf_result result = cgltf_parse_file(&options, path, &newParsedData);
    if(result == cgltf_result_success)
    {
        if(parsedData) {
            freeSceneGpuResources();
            cgltf_free(parsedData);
        }
        parsedData = newParsedData;
        cgltf_load_buffers(&options, parsedData, path);
        imgui_state::selectedSceneInd = -1;
        for(u32 i = 0; i < parsedData->scenes_count; i++)
            if(parsedData->scene == &parsedData->scenes[i]) {
                imgui_state::selectedSceneInd = i;
                break;
            }
        loadTextures();
        loadBufferObjects();
        createVaos();
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

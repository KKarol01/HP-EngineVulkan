#pragma once

#include <engine/handle.hpp>
#include <engine/shader.hpp>
#include <engine/model.hpp>
#include <engine/commandpool.hpp>
#include <engine/queue.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>
#include <memory>

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <vma/vma.h>

namespace eng {

class Window;
class Shader;
class PipelineManager;
class BufferManager;
class TextureManager;
struct Buffer;

struct FrameRenderResources {
    CommandPool cmdpool;
    vk::CommandBuffer cmdbuff;
    vk::Semaphore image_ready, rendering_done;
    vk::Fence in_flight_fence;
};

enum class VkQueueFamilyType {
    None, Graphics, Presentation, Transfer, Compute
};

struct VkQueueFamily {
    uint32_t family_index{0}, count{0};
};

struct VulkanObjects {
    vk::Instance instance;
    vk::DebugUtilsMessengerEXT dbg_msng;
    vk::SurfaceKHR surface;
    vk::PhysicalDevice pdev;
    vk::Device dev;
    std::unordered_map<VkQueueFamilyType, std::vector<VkQueueFamily>> queue_families;
    std::vector<Queue> queues;
    Queue *queue_graphics{}, *queue_presentation{};
    vk::SwapchainCreateInfoKHR swapchain_ci;
    vk::SwapchainKHR swapchain;
    std::vector<vk::Image> swapchain_images;
    std::vector<vk::ImageView> swapchain_views;
    Handle<Buffer> buffer_vertex, buffer_index;
    std::vector<FrameRenderResources> per_frame_render_data;
    VmaAllocator allocator;
};

//maybe switch to 64bits later
using gpu_index_t = int32_t;
struct GpuMesh {
    const Mesh *original{nullptr};
};

struct MeshInstance {
    vk::Pipeline pipeline{};
    vk::PipelineLayout pipeline_layout{};
    vk::DescriptorSet material_descriptor{};
    uint32_t mesh_idx{0};
    gpu_index_t instance_id{-1};
};

struct RendererUIObjects {
    vk::Pipeline pipeline;
    vk::DescriptorPool descpool;
    vk::Image game_image;
    vk::ImageView game_image_view;
    VmaAllocation game_image_alloc;
    VmaAllocationInfo game_image_alloci;
    vk::Sampler sampler;
    void *game_im_txt_id;
};

class Renderer {
public:
    Renderer(Window *window);
    ~Renderer() noexcept;
    void update();
    void add_object(const Model *model);

    bool is_properly_initialized() const { return _is_properly_initialized; }

private:
    [[nodiscard]] bool initialize_vulkan();
    [[nodiscard]] bool create_swapchain();
    [[nodiscard]] bool create_rendering_resources();
    [[nodiscard]] bool create_vma();
    [[nodiscard]] bool initialize_imgui();

    const std::vector<Shader>* get_or_create_shaders(const std::string &shader_name);
    void upload_meshes();
    void upload_mesh_instances();
    uint32_t get_frame_resource_index(int idx) const { return std::abs(idx % (int)_vk.per_frame_render_data.size()); }
    FrameRenderResources& get_frame_resources();

    Window *window{nullptr};
    VulkanObjects _vk;
    RendererUIObjects _ui;
    std::unique_ptr<BufferManager> buffer_mgr;
    std::unique_ptr<TextureManager> texture_mgr;
    std::unique_ptr<PipelineManager> ppmgr;
    std::unordered_map<std::string, std::vector<Shader>> shaders;
    std::vector<GpuMesh> meshes;
    std::vector<size_t> meshes_to_upload;
    std::vector<MeshInstance> mesh_instances;
    std::vector<size_t> mesh_instances_to_upload;
    bool _is_properly_initialized = false;
};

}
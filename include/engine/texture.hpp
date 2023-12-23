#pragma once

#include <engine/handle.hpp>

#include <unordered_map>
#include <filesystem>

#include <vulkan/vulkan.hpp>
#include <vma/vma.h>

namespace eng {

class BufferManager;
class Queue;

struct Texture : public Handle<Texture> {
    Texture(vk::Image image, vk::Format format, vk::ImageLayout current_layout, vk::ImageUsageFlags usage, std::vector<uint32_t> owning_queue_families, VmaAllocation allocation) noexcept 
        : Handle(GENERATE_HANDLE), image(image), format(format), current_layout(current_layout), usage(usage), owning_queue_families(owning_queue_families), allocation(allocation) {}
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture &&other) noexcept;
    Texture& operator=(Texture &&other) noexcept;

    vk::Image image{};
    vk::Format format{vk::Format::eUndefined};
    vk::ImageLayout current_layout{vk::ImageLayout::eUndefined};
    vk::ImageUsageFlags usage{};
    std::vector<uint32_t> owning_queue_families;
    VmaAllocation allocation{};
};

class TextureManager {
public:
    TextureManager(vk::Device device, BufferManager *buffer_mgr, VmaAllocator allocator) noexcept;
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;
    TextureManager(TextureManager &&other) noexcept;
    TextureManager& operator=(TextureManager &&other) noexcept;
    ~TextureManager() noexcept = default;

    Handle<Texture> allocate() const;
    Handle<Texture> load_from_file(std::filesystem::path file, Queue &queue, vk::CommandBuffer cmd, vk::ImageCreateInfo image_ci);
    vk::ImageView make_view(Handle<Texture> handle, vk::ImageViewCreateInfo view_ci) const;
    vk::Image get(Handle<Texture> handle) const; 

private:
    static void layout_transition(vk::CommandBuffer cmd, auto img, auto old_layout, auto new_layout, auto src_stage, auto src_access, auto dst_stage, auto dst_access, auto subresource) {
        vk::ImageMemoryBarrier img_barrier;
        img_barrier.setImage(img).setSrcAccessMask(src_access).setDstAccessMask(dst_access).setOldLayout(old_layout).setNewLayout(new_layout).setSubresourceRange(subresource);
        cmd.pipelineBarrier(src_stage, dst_stage, {}, {}, {}, img_barrier);
    };

    vk::Device device;
    BufferManager *buffer_mgr{};
    VmaAllocator allocator{};
    std::unordered_map<Handle<Texture>, Texture> textures;
    std::unordered_map<std::string, Handle<Texture>> texture_paths;
};

}


/*
    load image: path, maybe format
*/
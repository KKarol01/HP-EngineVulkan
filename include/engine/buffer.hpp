#pragma once

#include <engine/handle.hpp>
#include <engine/commandpool.hpp>

#include <vulkan/vulkan.hpp>
#include <vma/vma.h>

namespace eng {

class Queue;

struct Buffer : public Handle<Buffer> {
    Buffer() = default;
    Buffer(vk::Buffer buffer, vk::BufferUsageFlags usage, VmaAllocation allocation, std::span<const uint32_t> queue_families) 
        : Handle(GENERATE_HANDLE), buffer(buffer), usage(usage), allocation(allocation), queue_families(queue_families.begin(), queue_families.end()) { }
    
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    
    Buffer(Buffer &&other) noexcept : Handle(std::move(other)) {
        buffer = other.buffer;
        usage = other.usage;
        allocation = other.allocation;
        queue_families = other.queue_families;
        size = other.size;
        capacity = other.capacity;

        other.buffer = nullptr;
        other.size = 0;
        other.capacity = 0;
    }
    Buffer& operator=(Buffer &&other) noexcept {
        Handle::operator=(std::move(other));
        buffer = other.buffer;
        usage = other.usage;
        allocation = other.allocation;
        queue_families = other.queue_families;
        size = other.size;
        capacity = other.capacity;

        other.buffer = nullptr;
        other.size = 0;
        other.capacity = 0;

        return *this;
    }
    
    vk::Buffer buffer{};
    vk::BufferUsageFlags usage;
    VmaAllocation allocation{};
    std::vector<uint32_t> queue_families;
    size_t size{0}, capacity{0};
};

class BufferManager {
public:
    BufferManager(vk::Device device, VmaAllocator allocator, Queue *queue) noexcept;
    BufferManager(BufferManager&&) noexcept = default;
    BufferManager& operator=(BufferManager&&) noexcept = default;
    ~BufferManager() noexcept;

    [[nodiscard]] Handle<Buffer> allocate(const vk::BufferCreateInfo &buffer_ci, const VmaAllocationCreateInfo &allocation_ci, std::span<const std::byte> data = {});
    [[nodiscard]] bool insert(Handle<Buffer> dst, size_t offset, std::span<const std::byte> data);
    [[nodiscard]] bool append(Handle<Buffer> dst, std::span<const std::byte> data);
    [[nodiscard]] bool transfer(Handle<Buffer> src, Handle<Buffer> dst);
    [[nodiscard]] bool transfer_and_free(Handle<Buffer> src, Handle<Buffer> dst);
    void clear(Handle<Buffer> handle);
    void free(Handle<Buffer> handle);
    [[nodiscard]] vk::Buffer get(Handle<Buffer> handle) const;
    [[nodiscard]] size_t size(Handle<Buffer> handle) const;
    [[nodiscard]] size_t capacity(Handle<Buffer> handle) const;
    [[nodiscard]] void* get_mapped_data(Handle<Buffer> handle) const;

private:
    VmaAllocationInfo _vma_allocinfo(Handle<Buffer> handle) const;

    vk::Device _device;
    VmaAllocator _allocator{};
    Queue *_queue{};
    CommandPool _pool{};
    vk::CommandBuffer _buffer;

    std::unordered_map<Handle<Buffer>, Buffer> _buffers;
    // std::unordered_map<Handle<Buffer>, Signal<Handle<Buffer>>> _resize_callbacks;
};

}
#include <engine/buffer.hpp>
#include <engine/commandpool.hpp>
#include <engine/queue.hpp>

#include <span>

namespace eng {

BufferManager::BufferManager(vk::Device device, VmaAllocator allocator, Queue *queue) noexcept : _device{device}, _allocator{allocator}, _queue{queue} {
    _pool = CommandPool{device, vk::CommandPoolCreateFlagBits::eResetCommandBuffer, queue->family_index};
    _buffer = _pool.allocate_buffers(vk::CommandBufferLevel::ePrimary, 1).at(0);
}

BufferManager::~BufferManager() noexcept {
    for(auto &[h, b] : _buffers) {
        vmaDestroyBuffer(_allocator, b.buffer, b.allocation);
    }
}

Handle<Buffer> BufferManager::allocate(const vk::BufferCreateInfo &buffer_ci, const VmaAllocationCreateInfo &allocation_ci, std::span<const std::byte> data) {
    VkBuffer buffer = {};
    VmaAllocation vmaa = {};
    VmaAllocationInfo vmaai = {};
    
    if(buffer_ci.size < data.size_bytes()) {
        return Handle<Buffer>{};
    }

    vmaCreateBuffer(_allocator, (const VkBufferCreateInfo*)&buffer_ci, &allocation_ci, &buffer, &vmaa, &vmaai);

    if(!buffer || !vmaa) {
        return Handle<Buffer>{};
    }
    
    Buffer wrapped_buffer{vk::Buffer{buffer}, buffer_ci.usage, vmaa, std::span<const uint32_t>{buffer_ci.pQueueFamilyIndices, buffer_ci.queueFamilyIndexCount}};
    wrapped_buffer.capacity = buffer_ci.size;
    Handle<Buffer> handle = wrapped_buffer;
    _buffers.emplace(handle, std::move(wrapped_buffer));
    assert(handle);

    if(data.size_bytes() > 0) {
        if(!append(handle, data)) {
            free(handle);
            return Handle<Buffer>{};
        }
    }

    return handle;
}

bool BufferManager::insert(Handle<Buffer> dst, size_t offset, std::span<const std::byte> data) {
    auto &buffer = _buffers.at(dst);
    if(buffer.capacity - offset < data.size_bytes()) { return false; }

    auto buffer_data = get_mapped_data(dst);
    if(buffer_data) {
        memcpy(buffer_data, data.data(), data.size_bytes());
        return true;
    }

    if(!(buffer.usage & vk::BufferUsageFlagBits::eTransferDst)) { return false; }

    vk::BufferCreateInfo bci{{}, data.size_bytes(), vk::BufferUsageFlagBits::eTransferSrc, vk::SharingMode::eExclusive, _queue->family_index};
    VmaAllocationCreateInfo vmaaci{
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO
    };
    auto stage = allocate(bci, vmaaci, data);
    if(!stage) { return false; }

    try {
        _buffer.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        _buffer.copyBuffer(get(stage), buffer.buffer, vk::BufferCopy{0, offset, data.size_bytes()});
        _buffer.end();
    } catch (const std::exception &error) {
        return false;
    }

    auto res = _queue->submit_async({vk::SubmitInfo{{}, {}, _buffer, {}}});
    if(!res.first) { free(stage); return false; }
    res.second.wait();
    free(stage);
    buffer.size = offset + data.size();

    return true;
}

bool BufferManager::append(Handle<Buffer> dst, std::span<const std::byte> data) {
    return insert(dst, size(dst), data);
}

bool BufferManager::transfer(Handle<Buffer> src, Handle<Buffer> dst) {
    if(src == dst) { return true; }
    if(size(src) > capacity(dst)) { return false; }

    const auto mapped_src = get_mapped_data(src);
    auto mapped_dst = get_mapped_data(dst); 
    if(mapped_src && mapped_dst) {
        memcpy(mapped_dst, mapped_src, size(src));
        return true;
    }
    
    const auto &buffer_src = _buffers.at(src);
    auto &buffer_dst = _buffers.at(dst);

    if(!(buffer_src.usage & vk::BufferUsageFlagBits::eTransferSrc) || !(buffer_dst.usage & vk::BufferUsageFlagBits::eTransferDst)) {
        return false;
    }

    try {
        _buffer.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        _buffer.copyBuffer(buffer_src.buffer, buffer_dst.buffer, vk::BufferCopy{0, 0, buffer_src.size});
        _buffer.end();

        auto res = _queue->submit({vk::SubmitInfo{{}, {}, _buffer, {}}});
        if(!res) { return false; }
        buffer_dst.size = buffer_src.size;
    } catch(const std::exception &error) {
        return false;
    }
    return true;
}

bool BufferManager::transfer_and_free(Handle<Buffer> src, Handle<Buffer> dst) {
    if(transfer(src, dst)) {
        free(src);
        return true;
    }
    return false;
}

size_t BufferManager::size(Handle<Buffer> handle) const {
    return _buffers.at(handle).size;
}

size_t BufferManager::capacity(Handle<Buffer> handle) const {
    return _buffers.at(handle).capacity;
}

void BufferManager::free(Handle<Buffer> handle) {
    auto &b = _buffers.at(handle);
    vmaDestroyBuffer(_allocator, b.buffer, b.allocation);
    _buffers.erase(handle);
}

vk::Buffer BufferManager::get(Handle<Buffer> handle) const { return _buffers.at(handle).buffer; }

void* BufferManager::get_mapped_data(Handle<Buffer> handle) const {
    auto &buffer = _buffers.at(handle);
    VmaAllocationInfo vmaai{};
    vmaGetAllocationInfo(_allocator, buffer.allocation, &vmaai);
    return vmaai.pMappedData;
}

void BufferManager::clear(Handle<Buffer> handle) {
    _buffers.at(handle).size = 0;
}

}
#include <engine/commandpool.hpp>

namespace eng {

CommandPool::CommandPool(vk::Device dev, vk::CommandPoolCreateFlags flags, uint32_t queue_family_index): _dev(dev), _flags(flags), _qfidx(queue_family_index) {
    try {
        _pool = _dev.createCommandPool(vk::CommandPoolCreateInfo{_flags, _qfidx, nullptr});
    } catch (const std::exception &error) {
        // errors: out of host memory, out of device memory.
    }
}

CommandPool::CommandPool(CommandPool &&other) noexcept {
    *this = std::move(other);
}

CommandPool& CommandPool::operator=(CommandPool &&other) noexcept {
    _dev = other._dev;
    _flags = other._flags;
    _qfidx = other._qfidx;
    _pool = other._pool;
    other._pool = nullptr;
    return *this;
}

CommandPool::~CommandPool() noexcept {
    if(_pool) {
        _dev.destroyCommandPool(_pool);
    }
}

std::vector<vk::CommandBuffer> CommandPool::allocate_buffers(vk::CommandBufferLevel level, uint32_t count) {
    try {
        return _dev.allocateCommandBuffers(vk::CommandBufferAllocateInfo{_pool, level, count, nullptr});
    } catch (const std::exception &error) {
        //out of host memory, out of device memory
    }
    return {};
}

}
#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>

namespace eng {

class CommandPool {
public:
    CommandPool() = default;
    CommandPool(vk::Device dev, vk::CommandPoolCreateFlags flags, uint32_t queue_family_index);
    CommandPool(const CommandPool&) = delete;
    CommandPool& operator=(const CommandPool&) = delete;
    CommandPool(CommandPool &&other) noexcept;
    CommandPool& operator=(CommandPool &&other) noexcept;
    ~CommandPool() noexcept;

    explicit operator bool() const noexcept {
        return !!_pool;
    }

    std::vector<vk::CommandBuffer> allocate_buffers(vk::CommandBufferLevel level, uint32_t count);

private:
    vk::Device _dev{};
    vk::CommandPoolCreateFlags _flags{};
    uint32_t _qfidx{};
    vk::CommandPool _pool{};
};

}
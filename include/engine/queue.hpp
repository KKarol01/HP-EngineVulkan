#pragma once

#include <mutex>
#include <future>

namespace eng {

class Queue {
public:
    Queue(vk::Device device, vk::Queue queue, uint32_t family_index);
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;
    Queue(Queue &&other) noexcept;
    Queue& operator=(Queue &&other) noexcept;

    bool operator==(const Queue &other) const noexcept { return !!_queue && _queue == other._queue; }

    operator bool() const noexcept { return !!_queue; }
    
    [[nodiscard]] std::pair<bool, std::future<void>> submit_async(std::vector<vk::SubmitInfo> submits, vk::Fence fence = nullptr);

    [[nodiscard]] bool submit(const std::vector<vk::SubmitInfo> submits, vk::Fence fence = nullptr);
    
    void wait_idle() const { _queue.waitIdle(); }

    [[nodiscard]] vk::Queue get_vkqueue() const { return _queue; }

public:
    uint32_t family_index{};

private:
    vk::Device _device{};
    vk::Queue _queue{};
    std::unordered_set<VkCommandBuffer> _pending_buffers;
    std::mutex _pending_buffers_mutex;
};


}
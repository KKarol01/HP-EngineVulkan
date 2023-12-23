#include <engine/queue.hpp>

#include <vulkan/vulkan.hpp>

namespace eng {

    Queue::Queue(vk::Device device, vk::Queue queue, uint32_t family_index): family_index(family_index), _device(device), _queue(queue) { }
    Queue::Queue(Queue &&other) noexcept {
        *this = std::move(other);
    }
    Queue& Queue::operator=(Queue &&other) noexcept {
        family_index = other.family_index;
        _queue = other._queue;
        other._queue = nullptr;
        return *this;
    }

    [[nodiscard]] std::pair<bool, std::future<void>> Queue::submit_async(std::vector<vk::SubmitInfo> submits, vk::Fence fence) {
        return std::make_pair<bool, std::future<void>>(true, std::async(std::launch::async, [&, submits](){
            if(!fence) {
                try {
                    fence = _device.createFence(vk::FenceCreateInfo{});
                } catch (const std::exception &error) {
                    return;
                }
            }

            std::vector<VkCommandBuffer> buffers;
            std::unique_lock pending_buffers_lock{_pending_buffers_mutex};
            for(const auto &s : submits) {
                for(auto i=0u; i<s.commandBufferCount; ++i) {
                    buffers.push_back(s.pCommandBuffers[i]);
                    _pending_buffers.insert(s.pCommandBuffers[i]);
                }
            }
            pending_buffers_lock.unlock();
            
            bool dont_wait = false;
            try{
                _queue.submit(submits, fence);
            } catch(const std::exception &error) {
                dont_wait = true;
            }

            if(!dont_wait) {
                try {
                    [[maybe_unused]] const auto result = _device.waitForFences(fence, true, -1ULL);
                    // can be success or timeout. dont care about timeout.
                } catch (const std::exception &error) {
                    // can be: out host/device memory, device lost
                }
            }

            pending_buffers_lock.lock();
            for(const auto &b : buffers) {
                _pending_buffers.erase(b);
            }
            pending_buffers_lock.unlock();
        }));
    }

    [[nodiscard]] bool Queue::submit(const std::vector<vk::SubmitInfo> submits, vk::Fence fence) {
            try{
                _queue.submit(submits, fence);
            } catch(const std::exception &error) {
                return false;
            }
            return true;
    }

}
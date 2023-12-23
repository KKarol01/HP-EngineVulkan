#include <engine/texture.hpp>
#include <engine/buffer.hpp>
#include <engine/queue.hpp>

#include <span>
#include <ranges>
#include <string>
#include <iostream>

#include <stb/stb_image.h>
#include <fmt/core.h>
#include <fmt/ranges.h>

namespace eng {

Texture::Texture(Texture &&other) noexcept {
    *this = std::move(other);
}

Texture& Texture::operator=(Texture &&other) noexcept {
    image = other.image;
    format = other.format;
    current_layout = other.current_layout;
    usage = other.usage;
    owning_queue_families = std::move(other.owning_queue_families);
    allocation = other.allocation;
    other.image = nullptr;
    other.allocation = nullptr;
    return *this;
}

TextureManager::TextureManager(vk::Device device, BufferManager *buffer_mgr, VmaAllocator allocator) noexcept : device(device), buffer_mgr(buffer_mgr), allocator(allocator) {

}

TextureManager::TextureManager(TextureManager &&other) noexcept {
    *this = std::move(other);
}

TextureManager& TextureManager::operator=(TextureManager &&other) noexcept {
    buffer_mgr = other.buffer_mgr;
    allocator = other.allocator;
    textures = std::move(other.textures);
    texture_paths = std::move(other.texture_paths);
    other.buffer_mgr = nullptr;
    other.allocator = nullptr;
    return *this;
}

Handle<Texture> TextureManager::load_from_file(std::filesystem::path file, Queue &queue, vk::CommandBuffer cmd, vk::ImageCreateInfo image_ci) {
    if(file.empty() || !std::filesystem::exists(file) || !std::filesystem::is_regular_file(file)) {
        std::cerr << fmt::format("Provided path \"{}\" is not valid.", file.string());
        return Handle<Texture>{};
    }

    if(!(image_ci.usage & vk::ImageUsageFlagBits::eTransferDst)) {
        std::cerr << fmt::format("Image create info needs to have TransferDst flag!");
        return Handle<Texture>{};
    }
    
    if(auto it = texture_paths.find(file.string()); it != texture_paths.end()) {
        // don't compare for data in create info. it will be when it's needed.
        return textures.at(it->second);
    }    

    if(image_ci.queueFamilyIndexCount == 0) {
        image_ci.queueFamilyIndexCount = 1;
        image_ci.pQueueFamilyIndices = &queue.family_index;
    }
    if(!std::ranges::any_of(std::span(image_ci.pQueueFamilyIndices, image_ci.queueFamilyIndexCount), [&](auto &e) { return e == queue.family_index; })) {
        std::cerr << fmt::format("Provided Queue object, with family index: {}, does not belong in the owning queue family set provided with the image's create info: {}", queue.family_index, fmt::join(std::span(image_ci.pQueueFamilyIndices, image_ci.queueFamilyIndexCount), ", "));
        return Handle<Texture>{};
    }

    uint32_t desired_channels = 0;
    if(image_ci.format == vk::Format::eR8G8B8A8Srgb) {
        desired_channels = 4;
    } else {
        std::cerr << fmt::format("Requested texture format: \"{}\" is unsupported", vk::to_string(image_ci.format));
        return Handle<Texture>{};
    }

    int x{}, y{}, ch{};
    auto data = stbi_load(file.string().c_str(), &x, &y, &ch, desired_channels);
    image_ci.extent = vk::Extent3D{(uint32_t)x, (uint32_t)y, 1};

    if(!data) {
        std::cerr << fmt::format("Image could not be loaded.");
        return Handle<Texture>{};
    }

    Handle<Buffer> stage;
    VkImage image{};
    VmaAllocationCreateInfo image_aci{};
    VmaAllocation image_alloc{};
    auto prev_layout = image_ci.initialLayout;
    image_ci.initialLayout = vk::ImageLayout::eUndefined;
    vmaCreateImage(allocator, (VkImageCreateInfo*)&image_ci, &image_aci, &image, &image_alloc, nullptr);
    image_ci.initialLayout = prev_layout;

    static auto cleanup = [&] {
        if(data) { stbi_image_free(data); }  
        if(stage) { buffer_mgr->free(stage); }
        if(image) { vmaDestroyImage(allocator, image, image_alloc); }
        std::cerr << "Could not load texture";
        return Handle<Texture>{};
    };

    if(!image) {
        return cleanup();
    }
    
    vk::BufferCreateInfo bci{{}, (uint32_t)x*y*4, vk::BufferUsageFlagBits::eTransferSrc};
    VmaAllocationCreateInfo baci{.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, .usage = VMA_MEMORY_USAGE_AUTO};
    stage = buffer_mgr->allocate(bci, baci, std::as_bytes(std::span(data, x*y*ch)));

    if(!stage) {
        return cleanup();
    }

    try {
        cmd.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
        layout_transition(cmd, image, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits::eTopOfPipe, vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite, vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
        cmd.copyBufferToImage(buffer_mgr->get(stage), image, vk::ImageLayout::eTransferDstOptimal, vk::BufferImageCopy{{}, {}, {}, vk::ImageSubresourceLayers{vk::ImageAspectFlagBits::eColor, 0, 0, 1}, {0, 0, 0}, {(uint32_t)x, (uint32_t)y, 1}});
        layout_transition(cmd, image, vk::ImageLayout::eTransferDstOptimal, image_ci.initialLayout, vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eTransferWrite, vk::PipelineStageFlagBits::eFragmentShader, vk::AccessFlagBits::eShaderRead, vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
        cmd.end();
    } catch(const std::exception &error) {
        return cleanup();
    }

    if(auto res = queue.submit_async({vk::SubmitInfo{{}, {}, cmd}}); !res.first) {
        return cleanup();
    } else {
        res.second.wait();
    }

    // TODO GENERATE MIP MAPS
    if(image_ci.mipLevels > 1) {
        std::cout << "[WARNING] Mipmaps not supported yet";
    }

    stbi_image_free(data);
    buffer_mgr->free(stage);
    auto queue_families_span = std::span(image_ci.pQueueFamilyIndices, image_ci.queueFamilyIndexCount);
    Texture texture{image, image_ci.format, image_ci.initialLayout, image_ci.usage, {queue_families_span.begin(), queue_families_span.end()}, image_alloc};
    Handle<Texture> handle = texture;
    textures.emplace(handle, std::move(texture));
    texture_paths[file.string()] = handle;
    return handle;
}

vk::ImageView TextureManager::make_view(Handle<Texture> handle, vk::ImageViewCreateInfo view_ci) const {
    if(!textures.contains(handle)) { return nullptr; }

    try {
        view_ci.image = textures.at(handle).image;
        return device.createImageView(view_ci); 
    } catch(const std::runtime_error &error) {
        // errors: out of host/device memory, invalid opaque capture address khr
        std::cerr << fmt::format("Could not create image view");
        return nullptr;
    }
}

vk::Image TextureManager::get(Handle<Texture> handle) const {
    return textures.at(handle).image;
}

}
#include <engine/renderer.hpp>
#include <engine/engine.hpp>
#include <engine/window.hpp>
#include <engine/shader.hpp>
#include <engine/handle.hpp>
#include <engine/pipelinemanager.hpp>
#include <engine/queue.hpp>
#include <engine/buffer.hpp>
#include <engine/texture.hpp>
#include <engine/model_loader.hpp>

#include <vector>
#include <string>
#include <cstdint>
#include <iostream>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>
#include <fmt/core.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>


static constexpr auto layout_transition = [](vk::CommandBuffer cmd, auto img, auto old_layout, auto new_layout, auto src_stage, auto src_access, auto dst_stage, auto dst_access, auto subresource) {
    vk::ImageMemoryBarrier img_barrier;
    img_barrier.setImage(img).setSrcAccessMask(src_access).setDstAccessMask(dst_access).setOldLayout(old_layout).setNewLayout(new_layout).setSubresourceRange(subresource);
    cmd.pipelineBarrier(src_stage, dst_stage, {}, {}, {}, img_barrier);
};

namespace eng {

Renderer::Renderer(Window *window): window{window} {
    if(!initialize_vulkan()) {
        std::cerr << "Could not initialize Vulkan API";
        return;
    }

    if(!create_swapchain()) {
        std::cerr << "Could not create swapchain";
        return;
    }

    if(!create_vma()) {
        std::cerr << "Could not create VMA";
        return;
    }

    if(!create_rendering_resources()) {
        std::cerr << "Could not create rendering resources";
        return;
    }

    if(!initialize_imgui()) {
        std::cerr << "Could not initialize imgui";
        return;
    }

    _is_properly_initialized = true;
}

Renderer::~Renderer() noexcept {
    _vk.dev.waitIdle();
}

void Renderer::update() {
    const auto [window_width, window_height] = window->size_pixels;
    if(window_width == 0 || window_height == 0) {
        return;
    }

    auto &frame_data = get_frame_resources();

    if(!meshes_to_upload.empty()) {
        _vk.dev.waitIdle();
        upload_meshes();
    }

    if(!mesh_instances_to_upload.empty()) {
        _vk.dev.waitIdle();
        upload_mesh_instances();
    }

    if(window->resized) {
        _vk.dev.waitIdle();
        if(!create_swapchain()) {
            std::cerr << "Could not recreate swapchain";
            return;
        }
    }

    {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::SetNextWindowSize({(float)window_width, (float)window_height});
    ImGui::Begin("test", 0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
    if(ImGui::BeginMenuBar()) {
        if(ImGui::BeginMenu("Some menu")) {
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    const auto ph_csp = ImGui::GetCursorScreenPos();
    ImGui::SetCursorPos(ph_csp - ImGui::GetStyle().WindowPadding);
    const auto space = ImGui::GetContentRegionMax();
    ImGui::BeginChild("project hierarchy", {200.0f, space.y}, ImGuiChildFlags_Border);  
        ImGui::SeparatorText("Project hierarchy");
        ImGui::Text("asdfkjhl");
    ImGui::EndChild();

    ImGui::SameLine();

    const auto gw_mz = space - ImVec2{400.0f, 0.0f};
    ImGui::SetCursorPos(ImGui::GetCursorScreenPos() - ImGui::GetStyle().WindowPadding - ImVec2{ImGui::GetStyle().ChildBorderSize, 0.0f});
    ImGui::BeginChild("game window", gw_mz, ImGuiChildFlags_Border);
        ImGui::Image(_ui.game_im_txt_id, {1024.0f, 768.0f});
        if(window->file_dropped() && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceExtern)) {
            std::filesystem::path resource = window->payload;
            if(std::filesystem::is_regular_file(resource)) {
                const auto directory = resource.parent_path();
                auto geom = GeometryImporter::import(resource);
                if(geom) {
                    std::cout << "Loaded geometry with " << geom.meshes.size(); 
                    for(auto &m : geom.meshes) { 
                        if(!m.material.texture_paths.empty()) { m.material.shader_name = "default_textured"; }
                        else { m.material.shader_name = "main"; }
                    }    
                    Model *m = new Model{};
                    m->geometry = new Geometry{geom};
                    add_object(m);
                }

            }
            ImGui::EndDragDropSource();
        }
    ImGui::EndChild();
    ImGui::SameLine();
    const auto ii_mz = space - ImVec2{200.0f, 0.0f} - (space - ImVec2{400.0f, space.y}) + ImGui::GetStyle().WindowPadding;
    ImGui::SetCursorPos(ImGui::GetCursorScreenPos() - ImGui::GetStyle().WindowPadding - ImVec2{ImGui::GetStyle().ChildBorderSize, 0.0f});
        ImGui::BeginChild("inspector", ii_mz, ImGuiChildFlags_Border);
            ImGui::SeparatorText("AAAAAAAAAAA");
        ImGui::EndChild();
    ImGui::End();

    ImGui::Render();
    }
    

    const auto rendering_wait_result = _vk.dev.waitForFences(frame_data.in_flight_fence, true, -1ULL);
    const auto [swapchain_image_result, swapchain_image_index] = _vk.dev.acquireNextImageKHR(_vk.swapchain, -1ULL, frame_data.image_ready);
    if(rendering_wait_result != vk::Result::eSuccess) { throw std::runtime_error{"Renderer is stuck on frame."}; }
    if(swapchain_image_result != vk::Result::eSuccess) { throw std::runtime_error{"Swapchain is busy."}; }
    
    _vk.dev.resetFences(frame_data.in_flight_fence);
    
    auto &cmd = frame_data.cmdbuff;
    auto &img = _vk.swapchain_images.at(swapchain_image_index);
    cmd.begin(vk::CommandBufferBeginInfo{vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

    vk::RenderingInfo rendering_info;
    std::vector<vk::RenderingAttachmentInfo> color_attachments{
        vk::RenderingAttachmentInfo{_vk.swapchain_views.at(swapchain_image_index), vk::ImageLayout::eColorAttachmentOptimal, {}, {}, {}, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ClearColorValue{0.0f, 0.0f, 0.0f, 1.0f}},
        vk::RenderingAttachmentInfo{_ui.game_image_view, vk::ImageLayout::eColorAttachmentOptimal, {}, {}, {}, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::ClearColorValue{0.0f, 0.0f, 0.0f, 1.0f}}
    };
    
    rendering_info.setRenderArea(vk::Rect2D{{}, {1024, 768}})
        .setLayerCount(1)
        .setViewMask(0)
        .setColorAttachments(color_attachments.at(1));
    
    layout_transition(cmd, _ui.game_image, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite, vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}); 

    cmd.beginRendering(rendering_info);
    cmd.bindVertexBuffers(0, buffer_mgr->get(_vk.buffer_vertex), {0});
    cmd.bindIndexBuffer(buffer_mgr->get(_vk.buffer_index), 0, vk::IndexType::eUint32);
    uint32_t first_index = 0, vertex_offset = 0;
    const MeshInstance *prev_instance{};
    for(const auto &mi : mesh_instances) {
        if(!mi.pipeline) { continue; }
        cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, mi.pipeline);
        if(mi.material_descriptor) { cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, mi.pipeline_layout, 2, mi.material_descriptor, {}); }
        cmd.drawIndexed(meshes.at(mi.mesh_idx).original->vertex_indices.size(), 1, first_index, vertex_offset, 0);
        if(!prev_instance) { prev_instance = &mi; }
        else {
            if(mi.mesh_idx != prev_instance->mesh_idx) {
                first_index += meshes.at(mi.mesh_idx).original->vertex_indices.size();
                vertex_offset += meshes.at(mi.mesh_idx).original->vertex_positions.size();
            }
        }
    }
    cmd.endRendering();
    layout_transition(cmd, _ui.game_image, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite, vk::PipelineStageFlagBits::eFragmentShader, vk::AccessFlagBits::eShaderRead, vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}); 

    rendering_info.setRenderArea({{}, {window_width, window_height}});
    rendering_info.setColorAttachments(color_attachments.at(0));
    layout_transition(cmd, img, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eNone, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite, vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}); 
    cmd.beginRendering(rendering_info);
    cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, _ui.pipeline);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    cmd.endRendering();
    layout_transition(cmd, img, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::AccessFlagBits::eColorAttachmentWrite, vk::PipelineStageFlagBits::eBottomOfPipe, vk::AccessFlagBits::eNone, vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}); 
    cmd.end();

    vk::PipelineStageFlags wait_flags[]{vk::PipelineStageFlagBits::eColorAttachmentOutput};
    vk::SubmitInfo submit_info{frame_data.image_ready, wait_flags, cmd, frame_data.rendering_done};
    std::vector<vk::SubmitInfo> submit_infos{submit_info};
    if(!_vk.queue_graphics->submit(submit_infos, frame_data.in_flight_fence)) {
        std::cerr << "Problem with queue submit";
        return;
    }

    try {
        uint32_t image_indices[]{swapchain_image_index};
        [[maybe_unused]] const auto present_result = _vk.queue_presentation->get_vkqueue().presentKHR(vk::PresentInfoKHR{frame_data.rendering_done, _vk.swapchain, image_indices});
    } catch(const std::exception &error) {
        std::cerr << error.what();
    }
}

void Renderer::add_object(const Model *model) {
    if(!model || !model->geometry) { return; }

    for(const auto &gomesh : model->geometry->meshes) {
        const auto it = std::find_if(cbegin(meshes), cend(meshes), [&gomesh](const auto &e) { return e.original == &gomesh; });
        const auto is_already_processed = it != cend(meshes);

        uint32_t meshidx = 0;
        if(is_already_processed) {
            meshidx = std::distance(cbegin(meshes), it);
        } else {
            meshidx = meshes.size();
            meshes_to_upload.emplace_back(meshidx);
            meshes.emplace_back(&gomesh);
        }
        mesh_instances_to_upload.emplace_back(mesh_instances.size());
        mesh_instances.push_back(MeshInstance{.mesh_idx = meshidx});
    }
}

bool Renderer::initialize_vulkan() {
    std::vector<const char*> ireq_exts, ireq_layers, dreq_exts;

    uint32_t num_glfw_exts;
    auto *glfw_exts = glfwGetRequiredInstanceExtensions(&num_glfw_exts);
    for(auto i=0u; i<num_glfw_exts; ++i) { ireq_exts.push_back(glfw_exts[i]); }
    dreq_exts = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    #ifndef NDEBUG
        ireq_exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        ireq_layers.push_back("VK_LAYER_KHRONOS_validation");
    #endif

    vk::ApplicationInfo ai{"appname", VK_MAKE_VERSION(1, 0, 0), "enginename", VK_MAKE_VERSION(1, 0, 0), VK_MAKE_API_VERSION(0, 1, 3, 0)};
    vk::InstanceCreateInfo ici{{}, &ai, ireq_layers, ireq_exts};

    vk::Instance vkinstance;
    try {
        vkinstance = vk::createInstance(ici);
    } catch (const std::exception &err) {
        // Handle: incompatible driver, extension not present and layer not present
        return false;
    }

    vk::DebugUtilsMessengerEXT vkdbgmsngr;
    #ifndef NDEBUG
    static constexpr auto debug_messenger_callback = [](
        [[maybe_unused]] VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageTypes,
        [[maybe_unused]] const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        [[maybe_unused]] void *pUserData) {
            fmt::println("{}", pCallbackData->pMessage);
            return VK_FALSE;
    }; 

    using msgsev = vk::DebugUtilsMessageSeverityFlagBitsEXT;
    using msgtype = vk::DebugUtilsMessageTypeFlagBitsEXT;
    vkdbgmsngr = vkinstance.createDebugUtilsMessengerEXT(
        vk::DebugUtilsMessengerCreateInfoEXT{{},
            msgsev::eInfo | msgsev::eError | msgsev::eWarning | msgsev::eVerbose,
            msgtype::eGeneral | msgtype::ePerformance | msgtype::eValidation,
            debug_messenger_callback
        },
        nullptr,
        vk::DispatchLoaderDynamic{vkinstance, vkGetInstanceProcAddr});
    #endif

    VkSurfaceKHR vksurface;
    if(glfwCreateWindowSurface(vkinstance, window->get_glfwptr(), 0, &vksurface) != VK_SUCCESS) {
        return false;
    }

    vk::PhysicalDevice vkpdev;
    vk::PhysicalDeviceType vkpdevtype;
    std::unordered_map<VkQueueFamilyType, std::vector<VkQueueFamily>> vkpdev_qfamilies;
    for(const auto &pdev : vkinstance.enumeratePhysicalDevices()) {
        const auto props = pdev.getProperties();

        std::unordered_map<VkQueueFamilyType, std::vector<VkQueueFamily>> families;
        for(uint32_t qfidx = 0; const auto &qf : pdev.getQueueFamilyProperties()) {
            if(qf.queueFlags & vk::QueueFlagBits::eGraphics) {
                families[VkQueueFamilyType::Graphics].emplace_back(qfidx, qf.queueCount);
            }
            if(qf.queueFlags & vk::QueueFlagBits::eTransfer) {
                families[VkQueueFamilyType::Transfer].emplace_back(qfidx, qf.queueCount);
            }
            if(qf.queueFlags & vk::QueueFlagBits::eCompute) {
                families[VkQueueFamilyType::Compute].emplace_back(qfidx, qf.queueCount);
            }
            if(pdev.getSurfaceSupportKHR(qfidx, vksurface)) {
                families[VkQueueFamilyType::Presentation].emplace_back(qfidx, qf.queueCount);
            }
            ++qfidx;
        }

        if(!families.contains(VkQueueFamilyType::Presentation) || !families.contains(VkQueueFamilyType::Graphics)) {
            continue;
        }

        if(!vkpdev || (vkpdev && vkpdevtype != vk::PhysicalDeviceType::eDiscreteGpu && props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)) {
            vkpdev = pdev;
            vkpdevtype = props.deviceType;
            vkpdev_qfamilies = families;
        }
    }
    if(!vkpdev) {
        return false;
    }

    vk::PhysicalDeviceFeatures2 dev_features;
    vk::PhysicalDeviceDynamicRenderingFeatures dev_dynren_features;
    vk::PhysicalDeviceDescriptorIndexingFeatures dev_descind_features;
    dev_dynren_features.setDynamicRendering(true);
    dev_descind_features.setRuntimeDescriptorArray(true)
        .setDescriptorBindingVariableDescriptorCount(true)
        .setShaderSampledImageArrayNonUniformIndexing(true);
    dev_features.setPNext(&dev_dynren_features);
    dev_dynren_features.setPNext(&dev_descind_features);

    std::vector<vk::DeviceQueueCreateInfo> vkdev_qcis;
    float vk_qps[]{1.0f};
    const auto &vk_gqf = vkpdev_qfamilies.at(VkQueueFamilyType::Graphics).at(0);
    const auto &vk_pqf = vkpdev_qfamilies.at(VkQueueFamilyType::Presentation).at(0);
    vkdev_qcis.push_back(vk::DeviceQueueCreateInfo{{}, vk_gqf.family_index, 1, vk_qps});
    if(vk_gqf.family_index != vk_pqf.family_index) {
        vkdev_qcis.push_back(vk::DeviceQueueCreateInfo{{}, vk_pqf.family_index, 1, vk_qps});
    }
    vk::DeviceCreateInfo vkdev_ci{{}, vkdev_qcis, {}, dreq_exts, {}, &dev_features};
    vk::Device vkdev;
    try {
        vkdev = vkpdev.createDevice(vkdev_ci);
    } catch (const std::exception &error) {
        return false;
    }
    
    std::vector<vk::Queue> vkdev_qs;
    for(const auto &e : vkdev_qcis) {
        for(uint32_t i=0; i<e.queueCount; ++i) {
            vkdev_qs.push_back(vkdev.getQueue(e.queueFamilyIndex, i));
        }
    }
    if(vkdev_qs.empty()) {
        return false;
    }

    _vk.instance = vkinstance;
    _vk.dbg_msng = vkdbgmsngr;
    _vk.surface = vksurface;
    _vk.pdev = vkpdev;
    _vk.queue_families = std::move(vkpdev_qfamilies);
    _vk.dev = vkdev;
    _vk.queues.emplace_back(_vk.dev, vkdev_qs.at(0), vk_gqf.family_index);
    uint32_t queue_presentation_idx = 0;
    if(vk_gqf.family_index != vk_pqf.family_index) {
        _vk.queues.emplace_back(_vk.dev, vkdev_qs.at(1), vk_pqf.family_index);
        queue_presentation_idx = 1;
    }
    _vk.queue_graphics = &_vk.queues.at(0);
    _vk.queue_presentation = &_vk.queues.at(queue_presentation_idx);

    return true;
}

const std::vector<Shader>* Renderer::get_or_create_shaders(const std::string &shader_name) {
    if(auto it = shaders.find(shader_name); it != cend(shaders)) {
        return &it->second;
    }

    const std::filesystem::path shaders_dir = "assets/shaders/";
    for(const auto &e : std::filesystem::directory_iterator{shaders_dir}) {
        if(e.is_regular_file() && e.path().has_filename() && e.path().filename().string().starts_with(shader_name)) {
            shaders[shader_name].emplace_back(_vk.dev, e.path().string());
        }
    }
    
    return &shaders.at(shader_name);
}

bool Renderer::create_swapchain() {
    const auto [window_width, window_height] = window->size_pixels;

    const auto srf_caps = _vk.pdev.getSurfaceCapabilitiesKHR(_vk.surface);
    vk::Extent2D swapchain_image_extent{
        std::clamp(window_width, srf_caps.minImageExtent.width, srf_caps.maxImageExtent.width),
        std::clamp(window_height, srf_caps.minImageExtent.height, srf_caps.maxImageExtent.height)
    };
    if(srf_caps.currentExtent.width != std::numeric_limits<uint32_t>::max() && srf_caps.currentExtent.height != std::numeric_limits<uint32_t>::max()) {
        swapchain_image_extent.width = srf_caps.currentExtent.width;
        swapchain_image_extent.height = srf_caps.currentExtent.height;
    }

    vk::SharingMode swapchain_image_sharing{vk::SharingMode::eExclusive};
    std::vector<uint32_t> swapchain_queue_families{_vk.queue_families.at(VkQueueFamilyType::Graphics).at(0).family_index};
    if(_vk.queue_graphics != _vk.queue_presentation) {
        swapchain_image_sharing = vk::SharingMode::eConcurrent;
        swapchain_queue_families.push_back(_vk.queue_families.at(VkQueueFamilyType::Presentation).at(0).family_index);
    }

    const auto srf_formats = _vk.pdev.getSurfaceFormatsKHR(_vk.surface);
    if(std::find(cbegin(srf_formats), cend(srf_formats), vk::Format::eB8G8R8A8Srgb) == cend(srf_formats)) {
        throw std::runtime_error{"Swapchain doesn't support image format: B8G8R8A8Srgb"};
    }

    _vk.swapchain_ci = vk::SwapchainCreateInfoKHR{
        {},
        _vk.surface,
        2,
        vk::Format::eB8G8R8A8Srgb,
        vk::ColorSpaceKHR::eSrgbNonlinear,
        swapchain_image_extent,
        1,
        vk::ImageUsageFlagBits::eColorAttachment,
        swapchain_image_sharing,
        static_cast<uint32_t>(swapchain_queue_families.size()),
        swapchain_queue_families.data(),
        vk::SurfaceTransformFlagBitsKHR::eIdentity, // not required by the standard
        vk::CompositeAlphaFlagBitsKHR::eOpaque,
        vk::PresentModeKHR::eFifo,
        VK_TRUE
    };

    if(_vk.swapchain) {
        for(auto &v : _vk.swapchain_views) {
            _vk.dev.destroyImageView(v);
        }
        _vk.dev.destroySwapchainKHR(_vk.swapchain);
    }
    
    try {
        _vk.swapchain = _vk.dev.createSwapchainKHR(_vk.swapchain_ci);
    } catch (const std::exception &error) {
        _vk.swapchain = nullptr;
        _vk.swapchain_images.clear();
        _vk.swapchain_views.clear();
        return false;
    }
    _vk.swapchain_images = _vk.dev.getSwapchainImagesKHR(_vk.swapchain);
    _vk.swapchain_views.resize(_vk.swapchain_images.size());
    for(auto i=0u; i<_vk.swapchain_views.size(); ++i) {
        vk::ImageViewCreateInfo ivci{{}, _vk.swapchain_images[i], vk::ImageViewType::e2D, _vk.swapchain_ci.imageFormat, {}, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
        _vk.swapchain_views[i] = _vk.dev.createImageView(ivci);
    }

    return true;
}

bool Renderer::create_rendering_resources() {
    try {
        ppmgr = std::make_unique<PipelineManager>(_vk.dev);
        buffer_mgr = std::make_unique<BufferManager>(_vk.dev, _vk.allocator, _vk.queue_graphics);
        texture_mgr = std::make_unique<TextureManager>(_vk.dev, &*buffer_mgr, _vk.allocator);
        vk::BufferCreateInfo vertex_ci{{}, 1024*1024, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst};
        vk::BufferCreateInfo index_ci{{}, 1024*100, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst};
        VmaAllocationCreateInfo vertex_vmaaci{.usage = VMA_MEMORY_USAGE_AUTO};
        _vk.buffer_vertex = buffer_mgr->allocate(vertex_ci, vertex_vmaaci);
        _vk.buffer_index = buffer_mgr->allocate(index_ci, vertex_vmaaci);

        for(auto i=0llu; i<_vk.swapchain_images.size(); ++i) {
            auto cp = CommandPool{_vk.dev, vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer, _vk.queue_families.at(VkQueueFamilyType::Graphics).at(0).family_index};
            auto buff = cp.allocate_buffers(vk::CommandBufferLevel::ePrimary, 1).at(0);
            vk::Semaphore image_ready, rendering_done;
            image_ready = _vk.dev.createSemaphore({});
            rendering_done = _vk.dev.createSemaphore({});
            vk::Fence in_flight = _vk.dev.createFence(vk::FenceCreateInfo{vk::FenceCreateFlagBits::eSignaled});
            _vk.per_frame_render_data.emplace_back(std::move(cp), buff, image_ready, rendering_done, in_flight);
        }
    } catch (const std::exception &error) {
        return false;
    }
    return true;
}

bool Renderer::create_vma() {
    VmaAllocatorCreateInfo vmaaci{
        .physicalDevice = _vk.pdev,
        .device = _vk.dev,
        .instance = _vk.instance,
        .vulkanApiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0),
    };
    if(vmaCreateAllocator(&vmaaci, &_vk.allocator) != VK_SUCCESS) {
        return false;
    }

    return true;
}

bool Renderer::initialize_imgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO(); (void)io;
    for(int i=0; i<IM_ARRAYSIZE(ImGui::GetStyle().Colors); ++i) {
        ImGui::GetStyle().Colors[i].x = powf(ImGui::GetStyle().Colors[i].x, 2.2f);
        ImGui::GetStyle().Colors[i].y = powf(ImGui::GetStyle().Colors[i].y, 2.2f);
        ImGui::GetStyle().Colors[i].z = powf(ImGui::GetStyle().Colors[i].z, 2.2f);
    }

    std::vector<vk::DescriptorPoolSize> imguidpss{
        vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 5}
    };

    vk::DescriptorPoolCreateInfo imguidpci{vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 5, imguidpss};
    _ui.descpool = _vk.dev.createDescriptorPool(imguidpci);

    ImGui_ImplGlfw_InitForVulkan(window->get_glfwptr(), true);
    ImGui_ImplVulkan_InitInfo imguiii = {};
    imguiii.Instance = _vk.instance;
    imguiii.PhysicalDevice = _vk.pdev;
    imguiii.Device = _vk.dev;
    imguiii.QueueFamily = _vk.queue_families.at(VkQueueFamilyType::Graphics).at(0).family_index;
    imguiii.Queue = _vk.queue_graphics->get_vkqueue();
    imguiii.DescriptorPool = _ui.descpool;
    imguiii.MinImageCount = _vk.swapchain_ci.minImageCount;
    imguiii.ImageCount = _vk.swapchain_images.size();
    imguiii.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    imguiii.UseDynamicRendering = true;
    imguiii.ColorAttachmentFormat = VK_FORMAT_B8G8R8A8_SRGB;
    imguiii.CheckVkResultFn = [](auto res) {
        if(res != VK_SUCCESS) {
            fmt::println("Imgui error: {}", (int)res);
        }
    };
    ImGui_ImplVulkan_Init(&imguiii, 0);

    PipelineConfig imguippc{
        get_or_create_shaders("imgui"),
        {vk::DynamicState::eViewportWithCount, vk::DynamicState::eScissorWithCount},
        {{0, 32, vk::VertexInputRate::eVertex}}
    };
    auto imguipp = ppmgr->get_or_create_pipeline(imguippc);
    _ui.pipeline = imguipp.pipeline;

    const auto [window_width, window_height] = window->size_pixels;
    vk::ImageCreateInfo game_image_ci{
        {}, vk::ImageType::e2D, vk::Format::eB8G8R8A8Srgb,
        vk::Extent3D{window_width, window_height, 1}, 1, 1, vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
    };
    
    VmaAllocationCreateInfo game_image_vmaaci{
        .usage = VMA_MEMORY_USAGE_AUTO
    };
    vmaCreateImage(_vk.allocator, (const VkImageCreateInfo*)&game_image_ci, &game_image_vmaaci, (VkImage*)&_ui.game_image, &_ui.game_image_alloc, &_ui.game_image_alloci);
    vk::ImageViewCreateInfo game_image_view_ci{{}, _ui.game_image, vk::ImageViewType::e2D, vk::Format::eB8G8R8A8Srgb, {}, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
    _ui.game_image_view = _vk.dev.createImageView(game_image_view_ci);
    _ui.sampler = _vk.dev.createSampler(vk::SamplerCreateInfo{});
    _ui.game_im_txt_id = ImGui_ImplVulkan_AddTexture(_ui.sampler, _ui.game_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    return true;
}

void Renderer::upload_meshes() {
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    for(auto i=0u; i<meshes_to_upload.size(); ++i) {
        const auto idx = meshes_to_upload.at(i);
        auto &gpumesh = meshes.at(idx);
        
        for(auto i=0u; i<gpumesh.original->vertex_positions.size(); ++i) {
            vertices.push_back(gpumesh.original->vertex_positions[i].x);
            vertices.push_back(gpumesh.original->vertex_positions[i].y);
            vertices.push_back(gpumesh.original->vertex_positions[i].z);

            if(!gpumesh.original->vertex_normals.empty()) {
                vertices.push_back(gpumesh.original->vertex_normals[i].x);
                vertices.push_back(gpumesh.original->vertex_normals[i].y);
                vertices.push_back(gpumesh.original->vertex_normals[i].z);
            } else {
                vertices.push_back(0.0f);
                vertices.push_back(0.0f);
                vertices.push_back(0.0f);
            }

            if(!gpumesh.original->vertex_texture_coords.empty()) {
                vertices.push_back(gpumesh.original->vertex_texture_coords[i].x);
                vertices.push_back(gpumesh.original->vertex_texture_coords[i].y);
            } else {
                vertices.push_back(0.0f);
                vertices.push_back(0.0f);
            }
        }

        indices.insert(indices.end(), gpumesh.original->vertex_indices.begin(), gpumesh.original->vertex_indices.end());
    }
    meshes_to_upload = {};

    if(!buffer_mgr->insert(_vk.buffer_vertex, 0, std::as_bytes(std::span{vertices}))) {
        std::cerr << "error when writing to vertex buffer";
    }
    if(!buffer_mgr->insert(_vk.buffer_index, 0, std::as_bytes(std::span{indices}))) {
        std::cerr << "error when writing to index buffer";
    }
}

void Renderer::upload_mesh_instances() {
    for(auto i=0u; i<mesh_instances_to_upload.size(); ++i) {
        const auto idx = mesh_instances_to_upload.at(i);
        auto &meshinst = mesh_instances.at(idx);
        const auto &gpumesh = meshes.at(meshinst.mesh_idx);

        const std::vector<Shader>* materialshaders = get_or_create_shaders(gpumesh.original->material.shader_name);
        auto pipeline = ppmgr->get_or_create_pipeline(PipelineConfig{
            materialshaders,
            {vk::DynamicState::eScissorWithCount, vk::DynamicState::eViewportWithCount},
            {
                vk::VertexInputBindingDescription{0, sizeof(gpumesh.original->vertex_positions.at(0)) + sizeof(gpumesh.original->vertex_normals.at(0)) + sizeof(gpumesh.original->vertex_texture_coords.at(0)), vk::VertexInputRate::eVertex}
            }
        });
        meshinst.pipeline = pipeline.pipeline;
        meshinst.pipeline_layout = pipeline.layout;

        if(!gpumesh.original->material.texture_paths.empty()) {
            const auto poolsize = vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 1};
            auto pool = _vk.dev.createDescriptorPool(vk::DescriptorPoolCreateInfo{{}, 1, poolsize});

            const auto desc_layout = ppmgr->get_layout(pipeline.layout).desc_set_layout_handles.at(2);
            auto descset = _vk.dev.allocateDescriptorSets(vk::DescriptorSetAllocateInfo{pool, desc_layout});
            meshinst.material_descriptor = descset.at(0);

            if(gpumesh.original->material.texture_paths.contains(TextureType::Diffuse)) {
                auto &frame = get_frame_resources();
                vk::ImageCreateInfo image_ci{{}, vk::ImageType::e2D, vk::Format::eR8G8B8A8Srgb, {}, 1, 1, vk::SampleCountFlagBits::e1};
                image_ci.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
                image_ci.initialLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                auto image = texture_mgr->load_from_file(gpumesh.original->material.texture_paths.at(TextureType::Diffuse), *_vk.queue_graphics, frame.cmdbuff, image_ci);
                if(!image) {
                    std::cerr << fmt::format("Could not create texture");
                } else {
                    auto image_view = texture_mgr->make_view(image, vk::ImageViewCreateInfo{{}, {}, vk::ImageViewType::e2D, vk::Format::eR8G8B8A8Srgb, {}, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}});
                    vk::DescriptorImageInfo desc_ii{_ui.sampler, image_view, vk::ImageLayout::eShaderReadOnlyOptimal};
                    vk::WriteDescriptorSet write_dset{meshinst.material_descriptor, 0, 0, vk::DescriptorType::eCombinedImageSampler, desc_ii, {}, {}};
                    _vk.dev.updateDescriptorSets(write_dset, {});
                }

            }
        }
    }

    std::sort(begin(mesh_instances), end(mesh_instances), [](auto &a, auto &b) { 
        return std::tie(a.mesh_idx, a.pipeline, a.material_descriptor) < std::tie(b.mesh_idx, b.pipeline, b.material_descriptor);
    });

    for(auto i=0u; i<mesh_instances.size(); ++i) { mesh_instances.at(i).instance_id = i; }
    mesh_instances_to_upload = {};
}

FrameRenderResources& Renderer::get_frame_resources() { return _vk.per_frame_render_data.at(get_frame_resource_index(Engine::get_frame_number())); }

}
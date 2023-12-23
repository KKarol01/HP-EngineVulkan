#pragma once

#include <string_view>
#include <string>
#include <filesystem>
#include <map>

#include <engine/handle.hpp>

#include <vulkan/vulkan.hpp>

namespace eng {

enum class ShaderType { None, Vertex, Fragment, };

struct ShaderInterfaceVariable {
    uint32_t location, vecsize;
};

struct ShaderInterface {
    std::vector<ShaderInterfaceVariable> inputs, outputs;
};

struct ShaderBinding {
    operator vk::DescriptorSetLayoutBinding() const {
        return vk::DescriptorSetLayoutBinding{
            binding_idx, type, count, stage, nullptr
        };
    }
    
    uint32_t set_idx;
    uint32_t binding_idx;
    vk::DescriptorType type;
    uint32_t count;
    vk::ShaderStageFlags stage;
};

struct ShaderResources {
    ShaderInterface interface;
    std::vector<ShaderBinding> bindings;
    vk::PushConstantRange push_constants; 
};

class Shader : public Handle<Shader> {
public:
    Shader(vk::Device device, std::filesystem::path file_path);

    vk::ShaderStageFlagBits get_vk_stage() const {
        switch (type) {
            case ShaderType::Vertex:
                return vk::ShaderStageFlagBits::eVertex;
            case ShaderType::Fragment:
                return vk::ShaderStageFlagBits::eFragment;
            default:
                assert(false && "Conversion from ShaderType to VkShaderStage not implemented.");
        }
        return vk::ShaderStageFlagBits::eAll;
    }

    ShaderType type{ShaderType::None};
    std::filesystem::path path;
    vk::ShaderModule module;
    ShaderResources resources;
};

}
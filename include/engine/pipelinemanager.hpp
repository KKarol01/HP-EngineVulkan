#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>

namespace eng {

class Shader;
struct ShaderBinding;

struct PipelineSetLayout {
    PipelineSetLayout(
        vk::DescriptorSetLayout layout,
        const std::vector<vk::DescriptorSetLayoutBinding> &bindings
    ): layout(layout), bindings(bindings) {}

    vk::DescriptorSetLayout layout;
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
};

struct PipelineLayout {
    PipelineLayout(
        vk::PipelineLayout layout,
        const std::vector<vk::DescriptorSetLayout> &desc_set_layout_handles
    ): layout(layout), desc_set_layout_handles(desc_set_layout_handles) {}

    vk::PipelineLayout layout;
    std::vector<vk::DescriptorSetLayout> desc_set_layout_handles;
};

struct Pipeline {
    Pipeline(
        vk::Pipeline pipeline,
        vk::PipelineLayout layout_handle,
        const std::vector<Shader> *shaders
    ): pipeline(pipeline), 
        layout(layout_handle),
        shaders(shaders) {}
    
    vk::Pipeline pipeline{};
    vk::PipelineLayout layout{};
    const std::vector<Shader> *shaders{};
};

struct PipelineConfig {
    const std::vector<Shader>* shaders; 
    std::vector<vk::DynamicState> dynamic_states;
    std::vector<vk::VertexInputBindingDescription> input_bindings;
};


class PipelineManager {
public:
    explicit PipelineManager(vk::Device dev);

    Pipeline get_or_create_pipeline(const PipelineConfig &p);
    const PipelineLayout& get_layout(vk::PipelineLayout layout) const;
    
private:
    Pipeline _build_pipeline(const PipelineConfig &config);
    vk::PipelineLayout _find_or_build_pipeline_layout(const PipelineConfig &config);
    const std::vector<vk::DescriptorSetLayoutBinding>& _get_set_layout_bindings(vk::DescriptorSetLayout dsl) const;
    bool _are_pipeline_set_layouts_compatible(const std::vector<vk::DescriptorSetLayoutBinding> &set_a, const std::vector<vk::DescriptorSetLayoutBinding> &set_b) const;
    std::vector<ShaderBinding> _merge_shader_bindings(const PipelineConfig &config) const;
    std::vector<vk::PushConstantRange> _get_push_constant_ranges(const PipelineConfig &config) const;

    vk::Device _dev;
    std::vector<PipelineConfig> _configs;
    std::vector<PipelineSetLayout> _set_layouts;
    std::vector<PipelineLayout> _layouts;
    std::vector<Pipeline> _pipelines;
};

}
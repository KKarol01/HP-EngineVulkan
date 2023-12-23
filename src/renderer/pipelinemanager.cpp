#include <engine/pipelinemanager.hpp>
#include <engine/shader.hpp>

#include <span>
#include <ranges>

#include <fmt/core.h>

namespace eng {

PipelineManager::PipelineManager(vk::Device dev): _dev(dev) {}

Pipeline PipelineManager::get_or_create_pipeline(const PipelineConfig &p) {
    for(size_t i=0; i<_configs.size(); ++i) {
        const auto &_config = _configs.at(i);
        if(_config.shaders != p.shaders) {
            continue;
        }
        
        return _pipelines.at(i);
    }

    _configs.push_back(p);
    _pipelines.push_back(_build_pipeline(p));
    return _pipelines.back();
}

const PipelineLayout& PipelineManager::get_layout(vk::PipelineLayout layout) const {
    for(const auto &pl : _layouts) {
        if(pl.layout == layout) { return pl; }
    }
    return *_layouts.end();
}

Pipeline PipelineManager::_build_pipeline(const PipelineConfig &config) {
    std::vector<vk::PipelineShaderStageCreateInfo> graphicspp_stages_ci{ };
    std::vector<vk::VertexInputAttributeDescription> graphicspp_input_attributes{ };

    for(const auto &s : *config.shaders) {
        graphicspp_stages_ci.push_back(
            vk::PipelineShaderStageCreateInfo{{}, s.get_vk_stage(), s.module, "main"}
        );
        if(s.get_vk_stage() == vk::ShaderStageFlagBits::eVertex) {
            uint32_t offset = 0;
            for(const auto &input : s.resources.interface.inputs) {
                vk::Format type;
                uint32_t type_size = 0;
                switch(input.vecsize) {
                    case 2:
                        type = vk::Format::eR32G32Sfloat;
                        type_size = 2 * sizeof(float);
                        break;
                    case 3:
                        type = vk::Format::eR32G32B32Sfloat;
                        type_size = 3 * sizeof(float);
                        break;
                    case 4:
                        type = vk::Format::eR32G32B32A32Sfloat;
                        type_size = 4 * sizeof(float);
                        break;
                    default:
                        fmt::println("Unrecognized shader interface input type of vecsize: {}", input.vecsize);
                        continue;
                }
                graphicspp_input_attributes.emplace_back(input.location, 0, type, offset);
                offset += type_size;
            }
        }
    }

    vk::PipelineVertexInputStateCreateInfo graphicspp_input_state_ci{{}, config.input_bindings, graphicspp_input_attributes};

    vk::PipelineInputAssemblyStateCreateInfo graphicspp_input_assembly_ci{{}, vk::PrimitiveTopology::eTriangleList, false};

    vk::PipelineTessellationStateCreateInfo graphicspp_tesselation_ci{{}, 1};

    vk::PipelineViewportStateCreateInfo graphicspp_viewport_ci{};

    vk::PipelineRasterizationStateCreateInfo graphicspp_rasterization_ci{{}, false, false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise, false, 0.0f, 0.0f, 0.0f, 1.0f};

    vk::PipelineMultisampleStateCreateInfo graphicspp_multisample_ci{{}, vk::SampleCountFlagBits::e1, false, 0.0f};

    vk::PipelineDepthStencilStateCreateInfo graphicspp_depthstencil_ci{{}, false, false, vk::CompareOp::eNever};

    vk::PipelineColorBlendStateCreateInfo graphicspp_colorblend_ci{{}, false};

    vk::PipelineDynamicStateCreateInfo graphicspp_dynamic_ci{{}, config.dynamic_states};

    vk::PipelineLayout graphicspp_layout = _find_or_build_pipeline_layout(config);

    vk::GraphicsPipelineCreateInfo graphicspp_ci;
    graphicspp_ci.setStages(graphicspp_stages_ci)
        .setPVertexInputState(&graphicspp_input_state_ci)
        .setPInputAssemblyState(&graphicspp_input_assembly_ci)
        .setPTessellationState(&graphicspp_tesselation_ci)
        .setPViewportState(&graphicspp_viewport_ci)
        .setPRasterizationState(&graphicspp_rasterization_ci)
        .setPMultisampleState(&graphicspp_multisample_ci)
        .setPDepthStencilState(&graphicspp_depthstencil_ci)
        .setPColorBlendState(&graphicspp_colorblend_ci)
        .setPDynamicState(&graphicspp_dynamic_ci)
        .setLayout(graphicspp_layout);
    auto [result, graphicspp] = _dev.createGraphicsPipeline({}, graphicspp_ci);
    
    if(result != vk::Result::eSuccess) {
        throw std::runtime_error{"Could not create vk graphics pipeline."};
    }

    return Pipeline{graphicspp, graphicspp_layout, config.shaders};
}

vk::PipelineLayout PipelineManager::_find_or_build_pipeline_layout(const PipelineConfig &config) {
    const auto merged_bindings = _merge_shader_bindings(config);
    std::map<uint32_t, std::vector<ShaderBinding>> shader_sets;
    std::map<uint32_t, std::vector<vk::DescriptorSetLayoutBinding>> shader_vk_sets;
    uint32_t max_shader_set_idx = 0;
    for(const auto &e : merged_bindings) { shader_sets[e.set_idx].push_back(e); }
    for(auto &[shader_set_idx, shader_bindings] : shader_sets) { 
        std::sort(begin(shader_bindings), end(shader_bindings), [](auto &a, auto &b) { return a.binding_idx < b.binding_idx; });
        shader_vk_sets[shader_set_idx] = {shader_bindings.begin(), shader_bindings.end()};
        max_shader_set_idx = std::max(max_shader_set_idx, shader_set_idx);
    }

    // if pipeline doesn't have that many sets or if that particular set is null, 
    // mark as incompatible as a whole, but still check every set if it is reusable.
    std::map<uint32_t, vk::DescriptorSetLayout> matching_layouts;
    for(const auto &pl : _layouts) {
        bool is_compatible = true;
        for(auto &[shader_set_idx, shader_bindings] : shader_vk_sets) { 
            if(pl.desc_set_layout_handles.size() <= shader_set_idx || !pl.desc_set_layout_handles.at(shader_set_idx)) {
                is_compatible = false;
                continue; 
            }

            const auto &pipeline_bindings = _get_set_layout_bindings(pl.desc_set_layout_handles.at(shader_set_idx));
            if(!_are_pipeline_set_layouts_compatible(pipeline_bindings, shader_bindings)) {
                is_compatible = false;
                continue;
            }

            matching_layouts[shader_set_idx] = pl.desc_set_layout_handles.at(shader_set_idx);
        }

        if(is_compatible) {
            return pl.layout;
        }
    }

    
    const auto push_constant_ranges = _get_push_constant_ranges(config);
    std::vector<vk::DescriptorSetLayout> set_layouts(max_shader_set_idx + 1);
    for(const auto &[idx, layout] : shader_vk_sets) {
        if(auto it = matching_layouts.find(idx); it != end(matching_layouts)) {
            set_layouts.at(idx) = it->second;
            continue;
        }

        vk::DescriptorSetLayoutCreateInfo info{{}, layout};
        set_layouts.at(idx) = _dev.createDescriptorSetLayout(info);
    }
    for(auto &e : set_layouts) {
        if(!e) {
            e = _dev.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{{}, 0, {}});
        }
    }

    vk::PipelineLayoutCreateInfo pplci{{}, set_layouts, push_constant_ranges};
    auto ppl = _dev.createPipelineLayout(pplci);
    return _layouts.emplace_back(ppl, set_layouts).layout;
}

const std::vector<vk::DescriptorSetLayoutBinding>& PipelineManager::_get_set_layout_bindings(vk::DescriptorSetLayout dsl) const {
    for(const auto &psl : _set_layouts) {
        if(psl.layout == dsl) { return psl.bindings; }
    }
    assert(false && "invalid vk::DescriptorSetLayout object");
    return _set_layouts.end()->bindings;
}

bool PipelineManager::_are_pipeline_set_layouts_compatible(const std::vector<vk::DescriptorSetLayoutBinding> &set_a, const std::vector<vk::DescriptorSetLayoutBinding> &set_b) const {
    const auto intersection = std::min(set_a.size(), set_b.size());
    for(auto i=0u; i<intersection; ++i) {
        const auto &a = set_a.at(i);
        const auto &b = set_b.at(i);
        if(a.binding != b.binding
            || a.descriptorCount != b.descriptorCount
            || a.descriptorType != b.descriptorType
            || a.stageFlags != b.stageFlags
        ) { return false; }
    }
    return true;
}

std::vector<ShaderBinding> PipelineManager::_merge_shader_bindings(const PipelineConfig &config) const {
    std::vector<ShaderBinding> merged;
    for(const auto &shader : *config.shaders) {
        for(const auto &sb : shader.resources.bindings) {
            auto it = std::find_if(begin(merged), end(merged), [&](auto &mb) { return mb.set_idx == sb.set_idx && mb.binding_idx == sb.binding_idx; });
            if(it != end(merged)) { 
                //Collision. Check for compatibility
                if(it->type != sb.type || it->count != sb.count) {
                    throw std::runtime_error{"Shader resources are not compatible."};
                }
                it->stage |= sb.stage;
                continue;
            }

            merged.push_back(sb);
        }
    }

    std::sort(begin(merged), end(merged), [](auto &a, auto &b) {
        if(a.set_idx >= b.set_idx) { return false; }
        if(a.binding_idx >= b.binding_idx) { return false; }
        return true; 
    });

    return merged;
}

// std::vector<std::vector<vk::DescriptorSetLayoutBinding>> PipelineManager::_shader_bindings_to_vkdescs(const std::vector<ShaderBinding> &bindings) const {
//     std::unordered_set<uint32_t> unique_set_indices;
//     for(const auto &ss : bindings) { unique_set_indices.insert(ss.set_idx); }

//     std::vector<std::vector<vk::DescriptorSetLayoutBinding>> descs(unique_set_indices.size());
//     for(const auto &e : bindings) {
//         descs.at(e.set_idx).emplace_back(
//             e.binding_idx, e.type, e.count, e.stage, nullptr
//         );
//     }
//     return descs;
// }

/*
Should utilize `offset` property.
*/
std::vector<vk::PushConstantRange> PipelineManager::_get_push_constant_ranges(const PipelineConfig &config) const {
    for(const auto &s : *config.shaders) {
        if(s.resources.push_constants.size > 0) {
            return {s.resources.push_constants};
        }
    } 
    return {};
}

}
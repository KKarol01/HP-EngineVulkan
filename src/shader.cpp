#include <engine/shader.hpp>
#include <engine/file_reader.hpp>

#include <array>

#include <spirv_cross/spirv_cross.hpp>

namespace eng {

Shader::Shader(vk::Device device, std::filesystem::path file_path): Handle(GENERATE_HANDLE), path(file_path) {
    const auto file_data = FileReader::read(file_path, std::ios_base::binary | std::ios_base::in);

    vk::ShaderModuleCreateInfo smci{{}, file_data.size(), reinterpret_cast<const uint32_t*>(file_data.data())};
    
    module = device.createShaderModule(smci);
    if(const auto ext = std::filesystem::path{file_path}.replace_extension("").extension(); ext == ".vert") { 
        type = ShaderType::Vertex; 
    } else if(ext == ".frag") { 
        type = ShaderType::Fragment; 
    } else { assert(false && "Unrecognized shader type."); }

    spirv_cross::Compiler c{reinterpret_cast<const uint32_t*>(file_data.data()), file_data.size()/4};
    const auto shader_resouces = c.get_shader_resources();


    static constexpr std::array<vk::DescriptorType, 2> res_types_to_read{
        vk::DescriptorType::eStorageBuffer,
        vk::DescriptorType::eCombinedImageSampler,
    };

    for(const auto resource_type : res_types_to_read) {
        const spirv_cross::SmallVector<spirv_cross::Resource> *resvec{nullptr};

        switch (resource_type) {
            case vk::DescriptorType::eStorageBuffer:
                resvec = &shader_resouces.storage_buffers;
                break;
            case vk::DescriptorType::eCombinedImageSampler:
                resvec = &shader_resouces.sampled_images;
                break;
            default:
                assert(false && "Unhandled type");
                continue;

        }
        assert(resvec && "Resource vector cannot be nullptr");

        for(const auto &r : *resvec) {
            const auto decor_bitset = c.get_decoration_bitset(r.id);
            if(!decor_bitset.get(spv::DecorationDescriptorSet)) { continue; }
            if(!decor_bitset.get(spv::DecorationBinding)) { continue; }

            const auto desc_set = c.get_decoration(r.id, spv::DecorationDescriptorSet);
            const auto desc_binding = c.get_decoration(r.id, spv::DecorationBinding);
            const auto &desc_array = c.get_type(r.type_id).array;
            const auto desc_size = desc_array.size() > 0 ? desc_array[0] : 1;


            resources.bindings.emplace_back(
                ShaderBinding{
                    desc_set,
                    desc_binding, 
                    resource_type, 
                    desc_size, 
                    get_vk_stage()
                }
            );
        }
    }

    resources.push_constants.size = 0; 
    resources.push_constants.offset = 0; 
    resources.push_constants.stageFlags = get_vk_stage(); 
    if(shader_resouces.push_constant_buffers.size() > 0) {
        for(const auto &range : c.get_active_buffer_ranges(shader_resouces.push_constant_buffers[0].id)) {
            resources.push_constants.size += range.range;
        }
    }

    for(int i=0; i<2; ++i) {
        const spirv_cross::SmallVector<spirv_cross::Resource> *resource{nullptr};
        std::vector<ShaderInterfaceVariable> *interface{nullptr};
        if(i == 0) { 
            resource = &shader_resouces.stage_inputs; 
            interface = &resources.interface.inputs;
        }
        else if(i == 1) { 
            resource = &shader_resouces.stage_outputs;
            interface = &resources.interface.outputs;
        }

        for(const auto &in : *resource) {
            if(!c.get_decoration_bitset(in.id).get(spv::DecorationLocation)) { continue; }
            const auto location = c.get_decoration(in.id, spv::DecorationLocation); 
            const auto &basetype = c.get_type(in.base_type_id);
            const auto size = basetype.vecsize; 
            interface->emplace_back(location, size);
        }

        std::sort(begin(*interface), end(*interface), [](auto &a, auto &b) {
            return a.location < b.location;
        });
    } 
}

}
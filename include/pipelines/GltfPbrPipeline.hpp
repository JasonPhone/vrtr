#pragma once

#include "utils/vk/allocation.hpp"
#include "utils/vk/descriptors.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
namespace vrtr {
struct MaterialPipeline {
  VkPipeline pipeline;
  VkPipelineLayout layout;
};

enum class MaterialPass { BaseOpaque, BaseTransparent, Others };

struct GPUMaterialInstance {
  MaterialPass pass_type;
  MaterialPipeline *p_pipeline;
  VkDescriptorSet descriptor_set;
};

struct GltfPbrMaterial {
  MaterialPipeline opaque_pipeline;
  MaterialPipeline transparent_pipeline;
  struct MaterialConstants {
    glm::vec4 color_factors;
    glm::vec4 metal_rough_factors;
    /// 256 bytes padding.
    glm::vec4 padding[14];
  };
  struct MaterialResources {
    AllocatedImage color_image;
    VkSampler color_sampler;
    AllocatedImage metal_rough_image;
    VkSampler metal_rough_sampler;
    VkBuffer data_buffer;
    uint32_t data_buffer_offset;
  };

  VkDescriptorSetLayout ds_layout;
  DescriptorWriter writer;

  // void buildPipelines(Engine *engine);
  // void clearResources(VkDevice device);
  // MaterialInstance writeMaterial(VkDevice device, MaterialPass pass,
  //                                const MaterialResources &resources,
  //                                DescriptorAllocator &d_allocator);
};
}; // namespace vrtr
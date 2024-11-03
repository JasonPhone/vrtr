#pragma once
#include "utils/vk/common.hpp"

namespace vkutil {
bool loadShaderModule(const char *file_path, VkDevice device,
                      VkShaderModule *out_shader_module);
}

struct PipelineBuilder {
  std::vector<VkPipelineShaderStageCreateInfo> ci_shader_stages;
  VkPipelineInputAssemblyStateCreateInfo ci_input_asm;
  VkPipelineRasterizationStateCreateInfo ci_raster;
  VkPipelineColorBlendAttachmentState color_blend_attach;
  VkPipelineMultisampleStateCreateInfo ci_MS;
  VkPipelineLayout pipeline_layout;
  VkPipelineDepthStencilStateCreateInfo ci_depth_stencil;
  VkPipelineRenderingCreateInfo ci_render;
  VkFormat fmt_color_attach;

  PipelineBuilder() { clear(); }
  void clear();
  VkPipeline buildPipeline(VkDevice device);
  void setShaders(VkShaderModule vert, VkShaderModule frag);
  void setInputTopology(VkPrimitiveTopology topology);
  void setPolygonMode(VkPolygonMode mode);
  void setCullMode(VkCullModeFlags mode, VkFrontFace front);
  void setMultisamplingNone();
  void setColorAttachFormat(VkFormat format);
  void setDepthFormat(VkFormat format);
  void disableDepthTest();
  void enableDepthTest(bool enable_depth_write, VkCompareOp comp);
  /**
   * @note
   *  Blending logic:
   *    outColor = srcColor * srcBlendFactor <op> dstColor * dstBlendFactor.
   *  srcColor is what we are processing, dstColor is what already in the image.
   */
  void disableBlending();
  void enableBlendingAdd();
  void enableBlendingAlpha();
};
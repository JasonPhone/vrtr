#include "vk_pipelines.h"
#include <fstream>
#include "vk_initializers.h"

bool vkutil::loadShaderModule(const char *file_path, VkDevice device,
                              VkShaderModule *out_shader_module) {

  // Open the file with cursor at the end.
  std::ifstream file(file_path, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    fmt::print("Error reading shader file {}\n", file_path);
    return false;
  }
  // Find size of the file by looking up the location of the cursor.
  // Because the cursor is at the end, it gives the size directly in bytes.
  size_t fileSize = (size_t)file.tellg();
  // Spirv expects the buffer to be in uint32, so make sure to reserve a int.
  // Should be big enough for the entire file.
  std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
  // Put file cursor at beginning.
  file.seekg(0);
  // Load the entire file into the buffer.
  file.read((char *)buffer.data(), fileSize);
  file.close();

  // Create a new shader module, using the buffer we loaded.
  VkShaderModuleCreateInfo create_info = {};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.pNext = nullptr;
  create_info.codeSize = buffer.size() * sizeof(uint32_t); // In bytes.
  create_info.pCode = buffer.data();

  VkShaderModule shader_module;
  if (vkCreateShaderModule(device, &create_info, nullptr, &shader_module) !=
      VK_SUCCESS) {
    fmt::print("Error creating shader module from {}\n", file_path);
    return false;
  }
  *out_shader_module = shader_module;
  return true;
}

void PipelineBuilder::clear() {
  ci_shader_stages.clear();
  ci_input_asm = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
  ci_raster = {.sType =
                   VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
  color_blend_attach = {};
  ci_MS = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
  pipeline_layout = {};
  ci_depth_stencil = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
  ci_render = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
}

VkPipeline PipelineBuilder::buildPipeline(VkDevice device) {
  VkPipelineViewportStateCreateInfo ci_viewport = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .pNext = nullptr,
  };
  ci_viewport.viewportCount = 1;
  ci_viewport.scissorCount = 1;

  // No blending (transparent objects) by now.
  VkPipelineColorBlendStateCreateInfo ci_color_blend = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .pNext = nullptr,
  };
  ci_color_blend.logicOpEnable = VK_FALSE;
  ci_color_blend.logicOp = VK_LOGIC_OP_COPY;
  ci_color_blend.attachmentCount = 1;
  ci_color_blend.pAttachments = &color_blend_attach;

  // No need, vertex data is sent by array.
  VkPipelineVertexInputStateCreateInfo ci_vert_input = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .pNext = nullptr,
  };

  VkGraphicsPipelineCreateInfo ci_pipeline = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
  };
  // connect the renderInfo to the pNext extension mechanism
  ci_pipeline.pNext = &ci_render;
  ci_pipeline.stageCount = (uint32_t)ci_shader_stages.size();
  ci_pipeline.pStages = ci_shader_stages.data();
  ci_pipeline.pVertexInputState = &ci_vert_input;
  ci_pipeline.pInputAssemblyState = &ci_input_asm;
  ci_pipeline.pViewportState = &ci_viewport;
  ci_pipeline.pRasterizationState = &ci_raster;
  ci_pipeline.pMultisampleState = &ci_MS;
  ci_pipeline.pColorBlendState = &ci_color_blend;
  ci_pipeline.pDepthStencilState = &ci_depth_stencil;
  ci_pipeline.layout = pipeline_layout;

  VkDynamicState dy_state[] = {VK_DYNAMIC_STATE_VIEWPORT,
                               VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo ci_dynamic_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .pNext = nullptr,
  };
  ci_dynamic_state.pDynamicStates = dy_state;
  ci_dynamic_state.dynamicStateCount = 2;
  ci_pipeline.pDynamicState = &ci_dynamic_state;
  VkPipeline pipeline = VK_NULL_HANDLE;
  if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &ci_pipeline,
                                nullptr, &pipeline) != VK_SUCCESS) {
    fmt::println("Error creating pipeline.");
  }
  return pipeline;
}
void PipelineBuilder::setShaders(VkShaderModule vert, VkShaderModule frag) {
  ci_shader_stages.clear();
  ci_shader_stages.push_back(
      vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vert));
  ci_shader_stages.push_back(vkinit::pipelineShaderStageCreateInfo(
      VK_SHADER_STAGE_FRAGMENT_BIT, frag));
}
void PipelineBuilder::setInputTopology(VkPrimitiveTopology topology) {
  ci_input_asm.topology = topology;
  // Enable for triangle strip or line strip.
  ci_input_asm.primitiveRestartEnable = VK_FALSE;
}
void PipelineBuilder::setPolygonMode(VkPolygonMode mode) {
  ci_raster.polygonMode = mode;
  ci_raster.lineWidth = 1.f;
}
void PipelineBuilder::setCullMode(VkCullModeFlags mode, VkFrontFace front) {
  ci_raster.cullMode = mode;
  ci_raster.frontFace = front;
}
void PipelineBuilder::setMultisamplingNone() {
  ci_MS.sampleShadingEnable = VK_FALSE;
  // 1 spp, just as disabled.
  ci_MS.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  ci_MS.minSampleShading = 1.f;
  ci_MS.pSampleMask = nullptr;
  ci_MS.alphaToCoverageEnable = VK_FALSE;
  ci_MS.alphaToOneEnable = VK_FALSE;
}
void PipelineBuilder::disableBlending() {
  // Default write mask.
  color_blend_attach.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_blend_attach.blendEnable = VK_FALSE;
}
void PipelineBuilder::setColorAttachFormat(VkFormat format) {
  fmt_color_attach = format;
  // Connect the format.
  ci_render.colorAttachmentCount = 1;
  ci_render.pColorAttachmentFormats = &fmt_color_attach;
}
void PipelineBuilder::setDepthFormat(VkFormat format) {
  ci_render.depthAttachmentFormat = format;
}
void PipelineBuilder::disableDepthTest() {
  ci_depth_stencil.depthTestEnable = VK_FALSE;
  ci_depth_stencil.depthWriteEnable = VK_FALSE;
  ci_depth_stencil.depthCompareOp = VK_COMPARE_OP_NEVER;
  ci_depth_stencil.depthBoundsTestEnable = VK_FALSE;
  ci_depth_stencil.stencilTestEnable = VK_FALSE;
  ci_depth_stencil.front = {};
  ci_depth_stencil.back = {};
  ci_depth_stencil.minDepthBounds = 0.f;
  ci_depth_stencil.maxDepthBounds = 1.f;
}
/**
 *
 * @param comp If (a comp b) is true, a is near and pass the test.
 */
void PipelineBuilder::enableDepthTest(bool enable_depth_write,
                                      VkCompareOp comp) {
  ci_depth_stencil.depthTestEnable = VK_TRUE;
  ci_depth_stencil.depthWriteEnable = enable_depth_write ? VK_TRUE : VK_FALSE;
  ci_depth_stencil.depthCompareOp = comp;
  ci_depth_stencil.depthBoundsTestEnable = VK_FALSE;
  ci_depth_stencil.stencilTestEnable = VK_FALSE;
  ci_depth_stencil.front = {};
  ci_depth_stencil.back = {};
  ci_depth_stencil.minDepthBounds = 0.f;
  ci_depth_stencil.maxDepthBounds = 1.f;
}
void PipelineBuilder::enableBlendingAdd() {
  // outColor = srcColor.rgb * srcColor.a + dstColor.rgb * 1.0.
  color_blend_attach.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_blend_attach.blendEnable = VK_TRUE;
  color_blend_attach.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  color_blend_attach.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attach.colorBlendOp = VK_BLEND_OP_ADD;
  color_blend_attach.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attach.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  color_blend_attach.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::enableBlendingAlpha() {
  // outColor = srcColor.rgb * srcColor.a + dstColor.rgb * (1.0 - srcColor.a).
  color_blend_attach.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_blend_attach.blendEnable = VK_TRUE;
  color_blend_attach.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  color_blend_attach.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  color_blend_attach.colorBlendOp = VK_BLEND_OP_ADD;
  color_blend_attach.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  color_blend_attach.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  color_blend_attach.alphaBlendOp = VK_BLEND_OP_ADD;
}
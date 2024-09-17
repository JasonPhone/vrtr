#include "vk_initializers.h"

VkCommandPoolCreateInfo
vkinit::cmdPoolCreateInfo(uint32_t queue_family_idx,
                          VkCommandPoolCreateFlags flags /*= 0*/) {
  VkCommandPoolCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  info.pNext = nullptr;
  info.queueFamilyIndex = queue_family_idx;
  info.flags = flags;
  return info;
}

VkCommandBufferAllocateInfo vkinit::cmdBufferAllocInfo(VkCommandPool pool,
                                                       uint32_t count /*= 1*/) {
  VkCommandBufferAllocateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  info.pNext = nullptr;

  info.commandPool = pool;
  info.commandBufferCount = count;
  info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  return info;
}

VkCommandBufferBeginInfo
vkinit::cmdBufferBeginInfo(VkCommandBufferUsageFlags flags /*= 0*/) {
  VkCommandBufferBeginInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  info.pNext = nullptr;

  info.pInheritanceInfo = nullptr;
  info.flags = flags;
  return info;
}
//< init_cmd_draw

//> init_sync
VkFenceCreateInfo vkinit::fenceCreateInfo(VkFenceCreateFlags flags /*= 0*/) {
  VkFenceCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  info.pNext = nullptr;

  info.flags = flags;

  return info;
}

VkSemaphoreCreateInfo
vkinit::semaphoreCreateInfo(VkSemaphoreCreateFlags flags /*= 0*/) {
  VkSemaphoreCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  info.pNext = nullptr;
  info.flags = flags;
  return info;
}
//< init_sync

//> init_submit
VkSemaphoreSubmitInfo
vkinit::semaphoreSubmitInfo(VkPipelineStageFlags2 stage_mask,
                            VkSemaphore semaphore) {
  VkSemaphoreSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  submit_info.pNext = nullptr;
  submit_info.semaphore = semaphore;
  submit_info.stageMask = stage_mask;
  submit_info.deviceIndex = 0;
  submit_info.value = 1;

  return submit_info;
}

VkCommandBufferSubmitInfo vkinit::cmdBufferSubmitInfo(VkCommandBuffer cmd) {
  VkCommandBufferSubmitInfo info{};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  info.pNext = nullptr;
  info.commandBuffer = cmd;
  info.deviceMask = 0;

  return info;
}

VkSubmitInfo2 vkinit::submitInfo(VkCommandBufferSubmitInfo *cmd,
                                 VkSemaphoreSubmitInfo *signal_info,
                                 VkSemaphoreSubmitInfo *wait_info) {
  VkSubmitInfo2 info = {};
  info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  info.pNext = nullptr;

  info.waitSemaphoreInfoCount = wait_info == nullptr ? 0 : 1;
  info.pWaitSemaphoreInfos = wait_info;

  info.signalSemaphoreInfoCount = signal_info == nullptr ? 0 : 1;
  info.pSignalSemaphoreInfos = signal_info;

  info.commandBufferInfoCount = 1;
  info.pCommandBufferInfos = cmd;

  return info;
}
//< init_submit

VkPresentInfoKHR vkinit::presentInfo() {
  VkPresentInfoKHR info = {};
  info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  info.pNext = nullptr;

  info.swapchainCount = 0;
  info.pSwapchains = nullptr;
  info.pWaitSemaphores = nullptr;
  info.waitSemaphoreCount = 0;
  info.pImageIndices = nullptr;

  return info;
}

//> color_info
VkRenderingAttachmentInfo vkinit::attachmentInfo(
    VkImageView view, VkClearValue *clear,
    VkImageLayout layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/) {
  VkRenderingAttachmentInfo color_attachment{};
  color_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  color_attachment.pNext = nullptr;

  color_attachment.imageView = view;
  color_attachment.imageLayout = layout;
  color_attachment.loadOp =
      clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  if (clear) {
    color_attachment.clearValue = *clear;
  }

  return color_attachment;
}
//< color_info
//> depth_info
VkRenderingAttachmentInfo vkinit::depthAttachmentInfo(
    VkImageView view,
    VkImageLayout layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/) {
  VkRenderingAttachmentInfo depth_attachment{};
  depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  depth_attachment.pNext = nullptr;

  depth_attachment.imageView = view;
  depth_attachment.imageLayout = layout;
  depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  // If use reverse-z, 0 as far.
  depth_attachment.clearValue.depthStencil.depth = 1.f;

  return depth_attachment;
}
//< depth_info
//> render_info
VkRenderingInfo
vkinit::renderingInfo(VkExtent2D render_extent,
                      VkRenderingAttachmentInfo *color_attachment,
                      VkRenderingAttachmentInfo *depth_attachment) {
  VkRenderingInfo render_info{};
  render_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  render_info.pNext = nullptr;

  render_info.renderArea = VkRect2D{VkOffset2D{0, 0}, render_extent};
  render_info.layerCount = 1;
  render_info.colorAttachmentCount = 1;
  render_info.pColorAttachments = color_attachment;
  render_info.pDepthAttachment = depth_attachment;
  render_info.pStencilAttachment = nullptr;

  return render_info;
}
//< render_info
//> subresource
VkImageSubresourceRange
vkinit::imageSubresourceRange(VkImageAspectFlags aspect_mask) {
  // Could process part of image arrays or mipmap images.
  // But this (all levels and layers) is just fine.
  VkImageSubresourceRange sub_image{};
  sub_image.aspectMask = aspect_mask;
  sub_image.baseMipLevel = 0;
  sub_image.levelCount = VK_REMAINING_MIP_LEVELS;
  sub_image.baseArrayLayer = 0;
  sub_image.layerCount = VK_REMAINING_ARRAY_LAYERS;

  return sub_image;
}
//< subresource

VkDescriptorSetLayoutBinding vkinit::descriptorsetLayoutBinding(
    VkDescriptorType type, VkShaderStageFlags stage_flags, uint32_t binding) {
  VkDescriptorSetLayoutBinding setbind = {};
  setbind.binding = binding;
  setbind.descriptorCount = 1;
  setbind.descriptorType = type;
  setbind.pImmutableSamplers = nullptr;
  setbind.stageFlags = stage_flags;

  return setbind;
}

VkDescriptorSetLayoutCreateInfo
vkinit::descriptorsetLayoutCreateInfo(VkDescriptorSetLayoutBinding *bindings,
                                      uint32_t n_bindings) {
  VkDescriptorSetLayoutCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.pNext = nullptr;

  info.pBindings = bindings;
  info.bindingCount = n_bindings;
  info.flags = 0;

  return info;
}

VkWriteDescriptorSet
vkinit::writeDescriptorImage(VkDescriptorType type, VkDescriptorSet dst_set,
                             VkDescriptorImageInfo *image_info,
                             uint32_t binding) {
  VkWriteDescriptorSet write = {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.pNext = nullptr;

  write.dstBinding = binding;
  write.dstSet = dst_set;
  write.descriptorCount = 1;
  write.descriptorType = type;
  write.pImageInfo = image_info;

  return write;
}

VkWriteDescriptorSet
vkinit::writeDescriptorBuffer(VkDescriptorType type, VkDescriptorSet dst_set,
                              VkDescriptorBufferInfo *buffer_info,
                              uint32_t binding) {
  VkWriteDescriptorSet write = {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.pNext = nullptr;

  write.dstBinding = binding;
  write.dstSet = dst_set;
  write.descriptorCount = 1;
  write.descriptorType = type;
  write.pBufferInfo = buffer_info;

  return write;
}

VkDescriptorBufferInfo vkinit::bufferInfo(VkBuffer buffer, VkDeviceSize offset,
                                          VkDeviceSize range) {
  VkDescriptorBufferInfo binfo{};
  binfo.buffer = buffer;
  binfo.offset = offset;
  binfo.range = range;
  return binfo;
}

//> image_set
VkImageCreateInfo vkinit::imageCreateInfo(VkFormat format,
                                          VkImageUsageFlags usage_flags,
                                          VkExtent3D extent) {
  VkImageCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  info.pNext = nullptr;

  info.imageType = VK_IMAGE_TYPE_2D;

  info.format = format;
  info.extent = extent;

  info.mipLevels = 1;
  info.arrayLayers = 1;

  // for MSAA.
  // We will not be using it by default, so default it to 1 spp.
  info.samples = VK_SAMPLE_COUNT_1_BIT;

  // Optimal tiling, which means the image is stored on the best gpu format
  // LINEAR tiling is better for CPU read-back but limits GPU optimizing.
  info.tiling = VK_IMAGE_TILING_OPTIMAL;
  info.usage = usage_flags;

  return info;
}

VkImageViewCreateInfo
vkinit::imageViewCreateInfo(VkFormat format, VkImage image,
                            VkImageAspectFlags aspect_flags) {
  // build a image-view for the depth image to use for rendering
  VkImageViewCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  info.pNext = nullptr;

  info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  info.image = image;
  info.format = format;
  info.subresourceRange.baseMipLevel = 0;
  info.subresourceRange.levelCount = 1;
  info.subresourceRange.baseArrayLayer = 0;
  info.subresourceRange.layerCount = 1;
  info.subresourceRange.aspectMask = aspect_flags;

  return info;
}
//< image_set
VkPipelineLayoutCreateInfo vkinit::pipelineLayoutCreateInfo() {
  VkPipelineLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  info.pNext = nullptr;

  // empty defaults
  info.flags = 0;
  info.setLayoutCount = 0;
  info.pSetLayouts = nullptr;
  info.pushConstantRangeCount = 0;
  info.pPushConstantRanges = nullptr;
  return info;
}

VkPipelineShaderStageCreateInfo
vkinit::pipelineShaderStageCreateInfo(VkShaderStageFlagBits stage,
                                      VkShaderModule shader_module,
                                      const char *entry) {
  VkPipelineShaderStageCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  info.pNext = nullptr;

  // shader stage
  info.stage = stage;
  // module containing the code for this shader stage
  info.module = shader_module;
  // the entry point of the shader
  info.pName = entry;
  return info;
}
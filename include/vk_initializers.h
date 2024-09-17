#pragma once

#include "vk_types.h"

namespace vkinit {
VkCommandPoolCreateInfo cmdPoolCreateInfo(uint32_t queue_family_idx,
                                          VkCommandPoolCreateFlags flags = 0);
VkCommandBufferAllocateInfo cmdBufferAllocInfo(VkCommandPool pool,
                                              uint32_t count = 1);

VkCommandBufferBeginInfo
cmdBufferBeginInfo(VkCommandBufferUsageFlags flags = 0);
VkCommandBufferSubmitInfo cmdBufferSubmitInfo(VkCommandBuffer cmd);

VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags = 0);

VkSemaphoreCreateInfo semaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0);

VkSubmitInfo2 submitInfo(VkCommandBufferSubmitInfo *cmd,
                         VkSemaphoreSubmitInfo *signal_info,
                         VkSemaphoreSubmitInfo *wait_info);
VkPresentInfoKHR presentInfo();

VkRenderingAttachmentInfo attachmentInfo(
    VkImageView view, VkClearValue *clear,
    VkImageLayout layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/);

VkRenderingAttachmentInfo depthAttachmentInfo(
    VkImageView view,
    VkImageLayout layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/);

VkRenderingInfo renderingInfo(VkExtent2D render_extent,
                              VkRenderingAttachmentInfo *color_attachment,
                              VkRenderingAttachmentInfo *depth_attachment);

VkImageSubresourceRange imageSubresourceRange(VkImageAspectFlags aspect_mask);

VkSemaphoreSubmitInfo semaphoreSubmitInfo(VkPipelineStageFlags2 stage_mask,
                                          VkSemaphore semaphore);
VkDescriptorSetLayoutBinding
descriptorsetLayoutBinding(VkDescriptorType type,
                           VkShaderStageFlags stage_flags, uint32_t binding);
VkDescriptorSetLayoutCreateInfo
descriptorsetLayoutCreateInfo(VkDescriptorSetLayoutBinding *bindings,
                              uint32_t n_bindings);
VkWriteDescriptorSet writeDescriptorImage(VkDescriptorType type,
                                          VkDescriptorSet dst_set,
                                          VkDescriptorImageInfo *image_info,
                                          uint32_t binding);
VkWriteDescriptorSet writeDescriptorBuffer(VkDescriptorType type,
                                           VkDescriptorSet dst_set,
                                           VkDescriptorBufferInfo *buffer_info,
                                           uint32_t binding);
VkDescriptorBufferInfo bufferInfo(VkBuffer buffer, VkDeviceSize offset,
                                  VkDeviceSize range);

VkImageCreateInfo imageCreateInfo(VkFormat format,
                                  VkImageUsageFlags usage_flags,
                                  VkExtent3D extent);
VkImageViewCreateInfo imageViewCreateInfo(VkFormat format, VkImage image,
                                          VkImageAspectFlags aspect_flags);
VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo();
VkPipelineShaderStageCreateInfo
pipelineShaderStageCreateInfo(VkShaderStageFlagBits stage,
                              VkShaderModule shader_module,
                              const char *entry = "main");
} // namespace vkinit
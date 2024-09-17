#pragma once

#include <vulkan/vulkan.h>

namespace vkutil {
void transitionImage(VkCommandBuffer cmd, VkImage image,
                     VkImageLayout cur_layout, VkImageLayout new_layout);
void copyImage(VkCommandBuffer cmd, VkImage src, VkImage dst,
               VkExtent2D src_size, VkExtent2D dst_size);
void generateMipmap(VkCommandBuffer cmd, VkImage image, VkExtent2D image_size);
} // namespace vkutil
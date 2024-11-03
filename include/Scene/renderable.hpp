#pragma once
#include "vulkan/vulkan.h"
// #include "utils/vk/allocation.hpp"

struct RenderObject {
// uint32_t

VkBuffer index_buffer;
uint32_t index_offset;
uint32_t index_count;
VkBuffer vertex_buffer;
};
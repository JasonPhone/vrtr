#pragma once

#include "utils/vk/common.hpp"
#include "utils/vk/allocation.hpp"

namespace vkbuffer {
class BufferBuilder {
public:
  BufferBuilder() {
    m_ci_buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    m_ci_buffer.pNext = nullptr;
    m_ci_alloc.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
  }
  BufferBuilder &setSize(size_t alloc_size) {
    m_ci_buffer.size = alloc_size;
    return *this;
  }
  BufferBuilder &addBufferUsage(VkBufferUsageFlagBits usage) {
    m_ci_buffer.usage |= usage;
    return *this;
  }
  BufferBuilder &setMemoryUsage(VmaMemoryUsage usage) {
    m_ci_alloc.usage = usage;
    return *this;
  }
  AllocatedBuffer build(VmaAllocator &allocator) {
    AllocatedBuffer buffer;
    buffer.allocator = allocator;
    VK_CHECK(vmaCreateBuffer(allocator, &m_ci_buffer, &m_ci_alloc,
                             &buffer.buffer, &buffer.allocation,
                             &buffer.alloc_info));
    return buffer;
  }

private:
  VkBufferCreateInfo m_ci_buffer = {};
  VmaAllocationCreateInfo m_ci_alloc = {};
};
} // namespace vkbuffer
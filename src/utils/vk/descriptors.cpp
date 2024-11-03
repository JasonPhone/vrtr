#include "utils/vk/descriptors.hpp"

void DescriptorLayoutBuilder::addBinding(uint32_t binding,
                                         VkDescriptorType type) {
  VkDescriptorSetLayoutBinding new_bind{};
  new_bind.binding = binding;
  new_bind.descriptorCount = 1;
  new_bind.descriptorType = type;

  bindings.push_back(new_bind);
}

void DescriptorLayoutBuilder::clear() { bindings.clear(); }

VkDescriptorSetLayout
DescriptorLayoutBuilder::build(VkDevice device,
                               VkShaderStageFlags shader_stages, void *p_next,
                               VkDescriptorSetLayoutCreateFlags flags) {
  // Specify shader stages. One for all.
  for (auto &b : bindings) {
    b.stageFlags |= shader_stages;
  }

  VkDescriptorSetLayoutCreateInfo info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  info.pNext = p_next;
  info.pBindings = bindings.data();
  info.bindingCount = (uint32_t)bindings.size();
  info.flags = flags;

  VkDescriptorSetLayout set_layout;
  VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set_layout));

  return set_layout;
}
VkDescriptorPool DescriptorAllocator::getPool(VkDevice device) {
  VkDescriptorPool pool;
  if (!m_ready_pools.empty()) {
    pool = m_ready_pools.back();
    m_ready_pools.pop_back(); // May add back to this array or used array.
  } else {
    pool = createPool(device, m_n_sets_per_pool, m_ratios);
    m_n_sets_per_pool = nextSetsPerPool(m_n_sets_per_pool);
  }
  return pool;
}

VkDescriptorPool
DescriptorAllocator::createPool(VkDevice device, uint32_t n_max_set,
                                std::span<PoolSizeRatio> pool_ratios) {
  std::vector<VkDescriptorPoolSize> pool_sizes;
  for (auto &ratio : pool_ratios) {
    pool_sizes.push_back(VkDescriptorPoolSize{
        .type = ratio.type,
        .descriptorCount = uint32_t(ratio.ratio * n_max_set)});
  }
  VkDescriptorPoolCreateInfo ci_pool = {};
  ci_pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  ci_pool.flags = 0;
  ci_pool.maxSets = n_max_set;
  ci_pool.poolSizeCount = (uint32_t)pool_sizes.size();
  ci_pool.pPoolSizes = pool_sizes.data();
  VkDescriptorPool pool;
  vkCreateDescriptorPool(device, &ci_pool, nullptr, &pool);
  return pool;
}
void DescriptorAllocator::initPool(VkDevice device, uint32_t n_max_set,
                                   std::span<PoolSizeRatio> pool_ratios) {
  m_ratios.clear();
  for (auto &r : pool_ratios) {
    m_ratios.push_back(r);
  }
  m_n_sets_per_pool =
      n_max_set > kMaxNSetsPerPool ? kMaxNSetsPerPool : n_max_set;
  VkDescriptorPool pool = createPool(device, m_n_sets_per_pool, pool_ratios);
  m_n_sets_per_pool = nextSetsPerPool(m_n_sets_per_pool);
  m_ready_pools.push_back(pool);
}
void DescriptorAllocator::clearPools(VkDevice device) {
  for (auto &p : m_ready_pools)
    vkResetDescriptorPool(device, p, 0);
  for (auto &p : m_full_pools) {
    vkResetDescriptorPool(device, p, 0);
    m_ready_pools.push_back(p);
  }
  m_full_pools.clear();
}
void DescriptorAllocator::destroyPools(VkDevice device) {
  for (auto &p : m_ready_pools)
    vkDestroyDescriptorPool(device, p, nullptr);
  m_ready_pools.clear();
  for (auto &p : m_full_pools)
    vkDestroyDescriptorPool(device, p, nullptr);
  m_full_pools.clear();
}
VkDescriptorSet DescriptorAllocator::allocate(VkDevice device,
                                              VkDescriptorSetLayout layout,
                                              void *p_next) {
  auto pool = getPool(device);
  VkDescriptorSetAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.pNext = p_next;
  alloc_info.descriptorPool = pool;
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = &layout;

  VkDescriptorSet ds;
  VkResult result = vkAllocateDescriptorSets(device, &alloc_info, &ds);
  if (result == VK_ERROR_OUT_OF_POOL_MEMORY ||
      result == VK_ERROR_FRAGMENTED_POOL) {
    m_full_pools.push_back(pool);
    pool = getPool(device);
    alloc_info.descriptorPool = pool;
    // Retry just once.
    VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, &ds));
  }
  m_ready_pools.push_back(pool);
  return ds;
}
void DescriptorWriter::writeBuffer(int binding, VkBuffer buffer, size_t size,
                                   size_t offset, VkDescriptorType d_type) {
  auto &info = m_buffer_infos.emplace_back(VkDescriptorBufferInfo{
      .buffer = buffer, .offset = offset, .range = size});
  VkWriteDescriptorSet write = {.sType =
                                    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  write.dstBinding = binding;
  write.dstSet = VK_NULL_HANDLE;
  write.descriptorCount = 1;
  write.descriptorType = d_type;
  write.pBufferInfo = &info;

  m_writes.push_back(write);
}
void DescriptorWriter::writeImage(int binding, VkImageView image,
                                  VkSampler sampler, VkImageLayout image_layout,
                                  VkDescriptorType d_type) {
  auto &info = m_image_infos.emplace_back(VkDescriptorImageInfo{
      .sampler = sampler, .imageView = image, .imageLayout = image_layout});
  VkWriteDescriptorSet write = {.sType =
                                    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  write.dstBinding = binding;
  write.dstSet = VK_NULL_HANDLE;
  write.descriptorCount = 1;
  write.descriptorType = d_type;
  write.pImageInfo = &info;

  m_writes.push_back(write);
}
void DescriptorWriter::clear() {
  m_image_infos.clear();
  m_buffer_infos.clear();
  m_writes.clear();
}
void DescriptorWriter::updateDescriptorSet(VkDevice device,
                                           VkDescriptorSet d_set) {
  for (VkWriteDescriptorSet &write : m_writes)
    write.dstSet = d_set;
  vkUpdateDescriptorSets(device, (uint32_t)m_writes.size(), m_writes.data(), 0,
                         nullptr);
}
#pragma once
#include "utils/vk/common.hpp"
#include <span>
#include <deque>

/**
 * @note  A descriptor set contains multiple
 *        bindings, each bound with an image or buffer.
 *        The descriptor layout is used to allocate a
 *        descriptor set from descriptor pool.
 */

struct DescriptorLayoutBuilder {

  std::vector<VkDescriptorSetLayoutBinding> bindings;

  void addBinding(uint32_t binding, VkDescriptorType type);
  void clear();
  VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shader_stages,
                              void *p_next = nullptr,
                              VkDescriptorSetLayoutCreateFlags flags = 0);
};

/**
 * @brief Scalable allocator, managing multiple pools.
 */
class DescriptorAllocator {
public:
  struct PoolSizeRatio {
    VkDescriptorType type;
    float ratio; // Groups of the same type.
  };
  /**
   * @brief Create the first des pool.
   *
   * @param n_max_set Max number of descriptor sets one pool can hold.
   *                  Following pool size will increase on this value.
   * @param pool_ratios Number of certain type of bindings each set can hold.
   */
  void initPool(VkDevice device, uint32_t n_max_set,
                std::span<PoolSizeRatio> pool_ratios);
  void clearPools(VkDevice device);
  void destroyPools(VkDevice device);

  VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout,
                           void *p_next = nullptr);

private:
  VkDescriptorPool getPool(VkDevice device);
  VkDescriptorPool createPool(VkDevice device, uint32_t n_max_set,
                              std::span<PoolSizeRatio> pool_ratios);
  uint32_t nextSetsPerPool(uint32_t last_n) {
    // A naive grow strategy.
    uint32_t n = last_n * 1.5;
    return n > kMaxNSetsPerPool ? kMaxNSetsPerPool : n;
  }

  std::vector<PoolSizeRatio> m_ratios;
  std::vector<VkDescriptorPool> m_full_pools;
  std::vector<VkDescriptorPool> m_ready_pools;
  uint32_t m_n_sets_per_pool;
  static constexpr uint32_t kMaxNSetsPerPool = 4096;
};

class DescriptorWriter {
public:
  void writeImage(int binding, VkImageView image, VkSampler sampler,
                  VkImageLayout image_layout, VkDescriptorType d_type);
  void writeBuffer(int binding, VkBuffer buffer, size_t size, size_t offset,
                   VkDescriptorType d_type);
  void clear();
  void updateDescriptorSet(VkDevice device, VkDescriptorSet d_set);

private:
  std::deque<VkDescriptorImageInfo> m_image_infos;
  std::deque<VkDescriptorBufferInfo> m_buffer_infos;
  // Image and buffer writes all comes down here.
  std::vector<VkWriteDescriptorSet> m_writes;
};
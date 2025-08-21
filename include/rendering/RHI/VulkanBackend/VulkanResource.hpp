#pragma once

#include "rendering/RHI/RHIUtils.hpp"

#include <vulkan/vulkan.hpp>
#include <cstdint>
#include <memory>
#include <vector>
#include <SDL3/SDL.h>

namespace vrtr::rhi::vk {

class VulkanRHIBackend;

/// BaseResource


} // namespace vrtr::rhi::vk

// class VulkanRHICommandPool : public RHICommandPool {
// public:
//   VulkanRHICommandPool(const RHICommandPoolInfo &info,
//                        VulkanRHIBackend &backend);

//   RHIQueueRef GetQueue() { return info.queue; }

//   const VkCommandPool &GetHandle() { return handle; }

//   virtual void Destroy() override final;
//   virtual void *RawHandle() override final { return handle; };

// private:
//   VkCommandPool handle;
// };

// /// Shader, Buffer, Texture

// class VulkanRHIBuffer : public RHIBuffer {
// public:
//   VulkanRHIBuffer(const RHIBufferInfo &info, VulkanRHIBackend &backend);

//   const VkBuffer &GetHandle() { return handle; }

//   virtual void *Map() override final;
//   virtual void UnMap() override final;

//   virtual void Destroy() override final;
//   virtual void *RawHandle() override final { return handle; };

// private:
//   VkBuffer handle;

//   VmaAllocation allocation;
//   VmaAllocationInfo allocationInfo;

//   bool mapped = false;
//   void *pointer = nullptr;
// };

// class VulkanRHITexture : public RHITexture {
// public:
//   VulkanRHITexture(const RHITextureInfo &info, VulkanRHIBackend &backend,
//                    VkImage image = VK_NULL_HANDLE);

//   const VkImage &GetHandle() { return handle; }

//   virtual void Destroy() override final;
//   virtual void *RawHandle() override final { return handle; };

// private:
//   VkImage handle;

//   VmaAllocation allocation;
//   VmaAllocationInfo allocationInfo;
// };

// class VulkanRHITextureView : public RHITextureView {
// public:
//   VulkanRHITextureView(const RHITextureViewInfo &info,
//                        VulkanRHIBackend &backend);

//   const VkImageView &GetHandle() { return handle; }

//   virtual void Destroy() override final;
//   virtual void *RawHandle() override final { return handle; };

// private:
//   VkImageView handle;
// };

// class VulkanRHISampler : public RHISampler {
// public:
//   VulkanRHISampler(const RHISamplerInfo &info, VulkanRHIBackend &backend);

//   const VkSampler &GetHandle() { return handle; }

//   virtual void Destroy() override final;
//   virtual void *RawHandle() override final { return handle; };

// private:
//   VkSampler handle;
// };

// class VulkanRHIShader : public RHIShader {
// public:
//   VulkanRHIShader(const RHIShaderInfo &info, VulkanRHIBackend &backend);

//   VkPipelineShaderStageCreateInfo GetShaderStageCreateInfo();

//   const VkShaderModule &GetHandle() { return handle; }

//   virtual void Destroy() override final;
//   virtual void *RawHandle() override final { return handle; };

// private:
//   VkShaderModule handle;
// };

// class VulkanRHIShaderBindingTable : public RHIShaderBindingTable {
// public:
//   VulkanRHIShaderBindingTable(const RHIShaderBindingTableInfo &info,
//                               VulkanRHIBackend &backend);

//   const std::vector<VkPipelineShaderStageCreateInfo> &GetStages() {
//     return stages;
//   }
//   const std::vector<VkRayTracingShaderGroupCreateInfoKHR> &GetGroups() {
//     return groups;
//   }

//   uint32_t GetRayGenGroupSize() { return rayGenGroupSize; }
//   uint32_t GetHitGroupSize() { return hitGroupSize; }
//   uint32_t GetRayMissGroupSize() { return rayMissGroupSize; }

//   virtual void Destroy() override final;

// private:
//   std::vector<VkPipelineShaderStageCreateInfo> stages;
//   std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
//   uint32_t rayGenGroupSize = 0;
//   uint32_t hitGroupSize = 0;
//   uint32_t rayMissGroupSize = 0;
// };

// class VulkanRHITopLevelAccelerationStructure
//     : public RHITopLevelAccelerationStructure {
// public:
//   VulkanRHITopLevelAccelerationStructure(
//       const RHITopLevelAccelerationStructureInfo &info,
//       VulkanRHIBackend &backend);

//   const VkAccelerationStructureKHR &GetHandle() { return handle; }
//   VkDeviceAddress GetAddress() { return address; }

//   virtual void Update(const std::vector<RHIAccelerationStructureInstanceInfo>
//                           &instanceInfos) override final;

//   virtual void Destroy() override final;
//   virtual void *RawHandle() override final { return handle; };

// private:
//   VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
//   VkDeviceAddress address;
//   RHIBufferRef accelerationStructureBuffer; // 加速结构占用的内存
//   RHIBufferRef instanceBuffer;              // 实例信息内存
// };

// class VulkanRHIBottomLevelAccelerationStructure
//     : public RHIBottomLevelAccelerationStructure {
// public:
//   VulkanRHIBottomLevelAccelerationStructure(
//       const RHIBottomLevelAccelerationStructureInfo &info,
//       VulkanRHIBackend &backend);

//   const VkAccelerationStructureKHR &GetHandle() { return handle; }
//   VkDeviceAddress GetAddress() { return address; }

//   virtual void Destroy() override final;
//   virtual void *RawHandle() override final { return handle; };

// private:
//   VkAccelerationStructureKHR handle;
//   VkDeviceAddress address;
//   RHIBufferRef accelerationStructureBuffer; // 加速结构占用的内存
// };

// /// Descriptor set and binding.

// class VulkanRHIRootSignature : public RHIRootSignature {
// public:
//   struct SetInfo {
//     std::vector<VkDescriptorSetLayoutBinding> bindings;
//     VkDescriptorSetLayout layout;
//   };

//   VulkanRHIRootSignature(const RHIRootSignatureInfo &info,
//                          VulkanRHIBackend &backend);

//   virtual RHIDescriptorSetRef CreateDescriptorSet(uint32_t set) override
//   final;

//   const std::vector<SetInfo> &GetSetInfos() { return setInfos; }

//   virtual void Destroy() override final;

// private:
//   std::vector<SetInfo> setInfos;
// };

// class VulkanRHIDescriptorSet : public RHIDescriptorSet {
// public:
//   VulkanRHIDescriptorSet(VkDescriptorSetLayout setLayout,
//                          VulkanRHIBackend &backend);

//   virtual RHIDescriptorSet &UpdateDescriptor(
//       const RHIDescriptorUpdateInfo &descriptorUpdateInfo) override final;

//   const VkDescriptorSet &GetHandle() { return handle; }

//   virtual void Destroy() override final;
//   virtual void *RawHandle() override final { return handle; };

// private:
//   VkDescriptorSet handle;
// };

// /// Render pass and pipelines.

// class VulkanRHIRenderPass : public RHIRenderPass {
// public:
//   VulkanRHIRenderPass(const RHIRenderPassInfo &info, VulkanRHIBackend
//   &backend);

//   const VkRenderPass &GetHandle() { return handle; }
//   const VkFramebuffer &GetFrameBuffer() { return frameBuffer; }

//   virtual void Destroy() override final;
//   virtual void *RawHandle() override final { return handle; };

// private:
//   VkRenderPass handle;
//   VkFramebuffer frameBuffer;
// };

// class VulkanRHIGraphicsPipeline : public RHIGraphicsPipeline {
// public:
//   VulkanRHIGraphicsPipeline(const RHIGraphicsPipelineInfo &info,
//                             VulkanRHIBackend &backend);

//   VkPipelineLayout GetPipelineLayout() { return pipelineLayout; }

//   const VkPipeline &GetHandle() { return handle; }

//   void Bind(VkCommandBuffer commandBuffer);

//   virtual void Destroy() override final;
//   virtual void *RawHandle() override final { return handle; };

// private:
//   VkPipeline handle;
//   VkPipelineLayout pipelineLayout;

//   std::vector<VkVertexInputBindingDescription> bindingDescriptions;
//   std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
//   std::vector<VkPipelineColorBlendAttachmentState> blendStates;

//   // 默认开启的动态设置状态
//   std::vector<VkDynamicState> dynamicStates = {
//       VK_DYNAMIC_STATE_VIEWPORT,   VK_DYNAMIC_STATE_SCISSOR,
//       VK_DYNAMIC_STATE_LINE_WIDTH, VK_DYNAMIC_STATE_VERTEX_INPUT_EXT,
//       VK_DYNAMIC_STATE_DEPTH_BIAS,
//       // VK_DYNAMIC_STATE_BLEND_CONSTANTS,
//       // VK_DYNAMIC_STATE_DEPTH_BOUNDS,
//       // VK_DYNAMIC_STATE_STENCIL_REFERENCE
//   };

//   VkPipelineVertexInputStateCreateInfo
//   GetInputStateCreateInfo(const VertexInputStateInfo &vertexInputState);
//   VkPipelineInputAssemblyStateCreateInfo
//   GetPipelineInputAssemblyStateCreateInfo(const PrimitiveType
//   &primitiveType); VkPipelineViewportStateCreateInfo
//   GetPipelineViewportStateCreateInfo();
//   VkPipelineRasterizationStateCreateInfo
//   GetPipelineRasterizationStateCreateInfo(
//       const RHIRasterizerStateInfo &rasterizerState);
//   VkPipelineMultisampleStateCreateInfo
//   GetPipelineMultisampleStateCreateInfo();
//   VkPipelineColorBlendStateCreateInfo
//   GetPipelineColorBlendStateCreateInfo(const RHIBlendStateInfo &blendState,
//                                        uint32_t size);
//   VkPipelineDepthStencilStateCreateInfo
//   GetPipelineDepthStencilStateCreateInfo(
//       const RHIDepthStencilStateInfo &depthStencilState);
//   VkPipelineDynamicStateCreateInfo GetPipelineDynamicStateCreateInfo();

//   void
//   GetDynamicInputStateCreateInfo(const VertexInputStateInfo
//   &vertexInputState); std::vector<VkVertexInputAttributeDescription2EXT>
//       dynamicAttributeDescriptions;
//   std::vector<VkVertexInputBindingDescription2EXT>
//   dynamicBindingDescriptions;
// };

// class VulkanRHIComputePipeline : public RHIComputePipeline {
// public:
//   VulkanRHIComputePipeline(const RHIComputePipelineInfo &info,
//                            VulkanRHIBackend &backend);

//   VkPipelineLayout GetPipelineLayout() { return pipelineLayout; }

//   const VkPipeline &GetHandle() { return handle; }

//   void Bind(VkCommandBuffer commandBuffer);

//   virtual void Destroy() override final;
//   virtual void *RawHandle() override final { return handle; };

// private:
//   VkPipeline handle;
//   VkPipelineLayout pipelineLayout;
// };

// /// Sync structures.

// class VulkanRHIFence : public RHIFence {
// public:
//   VulkanRHIFence(bool signaled, VulkanRHIBackend &backend);

//   virtual void Wait() override final;

//   const VkFence &GetHandle() { return handle; }

//   virtual void Destroy() override final;
//   virtual void *RawHandle() override final { return handle; };

// private:
//   VkFence handle;
// };

// class VulkanRHISemaphore : public RHISemaphore {
// public:
//   VulkanRHISemaphore(VulkanRHIBackend &backend);

//   const VkSemaphore &GetHandle() { return handle; }

//   virtual void Destroy() override final;
//   virtual void *RawHandle() override final { return handle; };

// private:
//   VkSemaphore handle;
// };
#pragma once
#include "rendering/RHI/RHIUtils.hpp"
#include "rendering/RHI/RHITypes.hpp"
#include <queue>

namespace vrtr::rhi {
class RHIResourceBase {
public:
  RHIResourceBase() = delete;
  RHIResourceBase(RHIResourceType resourceType) : resourceType(resourceType) {};
  virtual ~RHIResourceBase() {};

  inline RHIResourceType GetType() { return resourceType; }

  virtual void *RawHandle() = 0;

private:
  RHIResourceType resourceType;
  uint32_t lastUseFrame = 0;

  virtual void Destroy() = 0;
};

/// Basic: queue, surface, swapchain, cmd list.

class RHIQueue : public RHIResourceBase {
public:
  RHIQueue(const RHIQueueInfo &info)
      : RHIResourceBase(RHIResourceType::Queue), info(info) {}

  virtual void WaitIdle() = 0;

protected:
  RHIQueueInfo info;
};

class RHISurface : public RHIResourceBase {
public:
  RHISurface() : RHIResourceBase(RHIResourceType::Surface) {};

  Extent2D GetExtent() const { return extent; }

protected:
  Extent2D extent;
};

class RHISwapchain : public RHIResourceBase {
public:
  RHISwapchain(const RHISwapchainInfo &info)
      : RHIResourceBase(RHIResourceType::Swapchain), info(info) {}

  virtual uint32_t GetCurrentFrameIndex() = 0;
  virtual RHITextureRef GetTexture(uint32_t index) = 0;
  virtual RHITextureRef GetNewFrame(RHIFenceRef fence,
                                    RHISemaphoreRef signalSemaphore) = 0;
  virtual void Present(RHISemaphoreRef waitSemaphore) = 0;

protected:
  RHISwapchainInfo info;
};

class RHICommandPool : public RHIResourceBase,
                       public std::enable_shared_from_this<RHICommandPool> {
public:
  RHICommandPool(const RHICommandPoolInfo &info)
      : RHIResourceBase(RHIResourceType::CommandPool), info(info) {}

  RHICommandListRef CreateCommandList(bool byPass = true);

protected:
  RHICommandPoolInfo info;

  std::queue<RHICommandContextRef> idleContexts = {};
  std::vector<RHICommandContextRef> allocatedContexts = {};

  void ReturnToPool(RHICommandContextRef commandContext) {
    idleContexts.push(commandContext);
  }
};

// // 缓冲，纹理，着色器，加速结构
// //
// ////////////////////////////////////////////////////////////////////////////////////////////////////////

// class RHIBuffer : public RHIResourceBase {
// public:
//   RHIBuffer(const RHIBufferInfo &info) : RHIResource(RHI_BUFFER), info(info)
//   {}

//   virtual void *Map() = 0;
//   virtual void UnMap() = 0;

//   inline const RHIBufferInfo &GetInfo() const { return info; }

// protected:
//   RHIBufferInfo info;
// };

// class RHITextureView : public RHIResourceBase {
// public:
//   RHITextureView(const RHITextureViewInfo &info)
//       : RHIResource(RHI_TEXTURE_VIEW), info(info) {}

//   inline const RHITextureViewInfo &GetInfo() const { return info; }

// protected:
//   RHITextureViewInfo info;
// };

// class RHITexture : public RHIResourceBase {
// public:
//   RHITexture(const RHITextureInfo &info)
//       : RHIResource(RHI_TEXTURE), info(info) {}

//   Extent3D MipExtent(uint32_t mipLevel);

//   inline const TextureSubresourceRange &GetDefaultSubresourceRange() const {
//     return defaultRange;
//   }
//   inline const TextureSubresourceLayers &GetDefaultSubresourceLayers() const
//   {
//     return defaultLayers;
//   }

//   inline const RHITextureInfo &GetInfo() const { return info; }

// protected:
//   RHITextureInfo info;

//   TextureSubresourceRange defaultRange = {};
//   TextureSubresourceLayers defaultLayers = {};
// };

// class RHISampler : public RHIResourceBase {
// public:
//   RHISampler(const RHISamplerInfo &info)
//       : RHIResource(RHI_SAMPLER), info(info) {}

//   inline const RHISamplerInfo &GetInfo() const { return info; }

// protected:
//   RHISamplerInfo info;
// };

// class RHIShader : public RHIResourceBase {
// public:
//   RHIShader(const RHIShaderInfo &info) : RHIResource(RHI_SHADER), info(info)
//   {
//     frequency = info.frequency;
//   }

//   ShaderFrequency GetFrequency() const { return frequency; }
//   const ShaderReflectInfo &GetReflectInfo() const { return reflectInfo; }
//   const RHIShaderInfo &GetInfo() const { return info; }

// private:
//   ShaderFrequency frequency;

// protected:
//   RHIShaderInfo info;
//   ShaderReflectInfo reflectInfo;
// };

// class RHIShaderBindingTable : public RHIResourceBase {
// public:
//   RHIShaderBindingTable(const RHIShaderBindingTableInfo &info)
//       : RHIResource(RHI_SHADER_BINDING_TABLE), info(info) {}

//   const RHIShaderBindingTableInfo &GetInfo() const { return info; }

// protected:
//   RHIShaderBindingTableInfo info;
// };

// class RHITopLevelAccelerationStructure : public RHIResourceBase {
// public:
//   RHITopLevelAccelerationStructure(
//       const RHITopLevelAccelerationStructureInfo &info)
//       : RHIResource(RHI_TOP_LEVEL_ACCELERATION_STRUCTURE), info(info) {}

//   virtual void Update(const std::vector<RHIAccelerationStructureInstanceInfo>
//                           &instanceInfos) = 0;

//   const RHITopLevelAccelerationStructureInfo &GetInfo() const { return info;
//   }

// protected:
//   RHITopLevelAccelerationStructureInfo info;
// };

// class RHIBottomLevelAccelerationStructure : public RHIResourceBase {
// public:
//   RHIBottomLevelAccelerationStructure(
//       const RHIBottomLevelAccelerationStructureInfo &info)
//       : RHIResource(RHI_BOTTOM_LEVEL_ACCELERATION_STRUCTURE), info(info) {}

//   const RHIBottomLevelAccelerationStructureInfo &GetInfo() const {
//     return info;
//   }

// protected:
//   RHIBottomLevelAccelerationStructureInfo info;
// };

// // 根签名，描述符
// //
// ////////////////////////////////////////////////////////////////////////////////////////////////////////

// class RHIRootSignature
//     : public RHIResourceBase // 对pipelinelayout, descriptorSetPool等的抽象
// {
// public:
//   RHIRootSignature(const RHIRootSignatureInfo &info)
//       : RHIResource(RHI_ROOT_SIGNATURE), info(info) {}

//   virtual RHIDescriptorSetRef CreateDescriptorSet(uint32_t set) = 0;

//   const RHIRootSignatureInfo &GetInfo() { return info; }

// protected:
//   RHIRootSignatureInfo info;
// };

// class RHIDescriptorSet : public RHIResourceBase {
// public:
//   RHIDescriptorSet() : RHIResource(RHI_DESCRIPTOR_SET) {}

//   virtual RHIDescriptorSet &
//   UpdateDescriptor(const RHIDescriptorUpdateInfo &descriptorUpdateInfo) = 0;

//   RHIDescriptorSet &UpdateDescriptors(
//       const std::vector<RHIDescriptorUpdateInfo> &descriptorUpdateInfos) {
//     for (auto &info : descriptorUpdateInfos)
//       UpdateDescriptor(info);
//     return *this;
//   };
// };

// // 管线状态
// //
// ////////////////////////////////////////////////////////////////////////////////////////////////////////

// class RHIRenderPass
//     : public RHIResourceBase //
//     在vulkan里相当于renderpass和framebuffer的整体抽象
// {
// public:
//   RHIRenderPass(const RHIRenderPassInfo &info)
//       : RHIResource(RHI_RENDER_PASS), info(info) {}

//   const RHIRenderPassInfo &GetInfo() { return info; }

// protected:
//   RHIRenderPassInfo info;
// };

// class RHIGraphicsPipeline : public RHIResourceBase {
// public:
//   RHIGraphicsPipeline(const RHIGraphicsPipelineInfo &info)
//       : RHIResource(RHI_GRAPHICS_PIPELINE), info(info) {}

//   const RHIGraphicsPipelineInfo &GetInfo() { return info; }

// protected:
//   RHIGraphicsPipelineInfo info;
// };

// class RHIComputePipeline : public RHIResourceBase {
// public:
//   RHIComputePipeline(const RHIComputePipelineInfo &info)
//       : RHIResource(RHI_COMPUTE_PIPELINE), info(info) {}

// protected:
//   RHIComputePipelineInfo info;
// };

// class RHIRayTracingPipeline : public RHIResourceBase {
// public:
//   RHIRayTracingPipeline(const RHIRayTracingPipelineInfo &info)
//       : RHIResource(RHI_RAY_TRACING_PIPELINE), info(info) {}

// protected:
//   RHIRayTracingPipelineInfo info;
// };

// // 同步
// //
// ////////////////////////////////////////////////////////////////////////////////////////////////////////

// class RHIFence : public RHIResourceBase {
// public:
//   RHIFence() : RHIResource(RHI_FENCE) {}

//   virtual void Wait() = 0;
// };

// class RHISemaphore : public RHIResourceBase {
// public:
//   RHISemaphore() : RHIResource(RHI_SEMAPHORE) {}
// };

} // namespace vrtr::rhi
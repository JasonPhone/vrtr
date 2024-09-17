# Issues

ImGui still needs dynamic rendering now?

How we pass data to and from shader?

Scene structure, material model and shading model are all to be explored.

Render pass and dynamic rendering.

# TODO

## Function

- [ ] detailed compute pipeline for data processing
- [ ] deferred rendering.
- [ ] GPU-driven, pbr ibl, cascaded shadow mapping.
- [ ] Reversed-z: projection mat, depth compare operator, depth attachment clear value.
- [ ] CVAR system for configurable renderer.
- [ ] Multithreading.

## Structure

- [ ] Clean code structure.
- [ ] tick-based rendering.
- [ ] Take Vulkan and engine stuff out of glTF loaders. Use a middle layer to map the data.

# Notes

DescriptorPool for ImGui may *change*, the destroy callback should use value capture.

vma causes too much compile warning, suppressed using `#pragma clang diagnostic` around header.

Using dynamic rendering instead of `VkRenderPass`. May not work on mobile device where tile rendering is common.

Reversed-z takes depth value 1(INF in glm::perspective()) as near plane and 0 as far.
Can mitigate z-fighting because
1) objects are "pushed back" to far plane through perspective projection;
2) IEEE754 float value has higher precision when its abs is small.

By now (1419b16) the descriptor set is used to bind output image of compute shader.

# Overall structure

Vulkan 是一套在 GPU 上利用 shader 处理数据的 API。

## GPU 抽象

首先，需要初始化一个实例来调用 Vulkan API。目前项目仅用这个实例初创建 SDL surface 和 内存管理器。

GPU 被视为一个拥有若干 queue family 的 physical device。
GPU 提供的功能被抽象为一系列 feature、extension 和 property，
每个 family 仅支持一部分命令，例如仅支持 compute shader 或仅支持数据传输。
根据需要的功能选择 GPU 和 queue family，然后创建 logical device 和 queue。
logical device 是 Vulkan 与 GPU 交互的主要锚点，而 queue 用于接收提交给 GPU 执行的命令序列。
以上对象在 `Engine::initVulkan()` 中初始化。

Vulkan: Data processing by shader on GPU.

- GPU abstraction
  - physical device and queue family
  - logical device and queue
  - command pool and command buffer
- shader
  - compute pipeline
  - graphics pipeline
- data
  - mesh, material and texture
  - buffer and image
  - connect data with shader: descriptor
  - small data
  - massive data
- CPU & GPU interaction
  - api instance
- CPU & GPU sync: fence and semaphore
- design choices
  - global or per-frame data, clear or reset a pool is faster than track its allocations
- others
  - result presentation: swapchain, surface and GUI
  - memory management
  - merge into a simulation system

api instance, physical device and queue family, logical device and queue.
swapchain, images, command pool and buffer, sync structures.
descriptor layout, descriptorset and descriptor pool.

## Engine::init()

Init all structures and data.

Engine is a thread-unsafe singleton, it's checked here.
SDL window is created, with window flags herd-coded here.

Engine::initVulkan():
build the Vulkan API instance, debug messenger.
Create the native window surface with Vulkan API. This is for platform abstraction.
Select GPU by hard-coded features, extensions and properties. We do no per-GPU check.
Create logical device, get graphics queue family and corresponding queue.
Create engine-wise vma memory allocator.
Create query pool for render profiling.
Remember to update the destroy queue.

Engine::initSwapchain():
Create swapchain.
Create images, we render contents to these images then copy them to swapchain.
`m_color_image` for final color from graphics pipeline or compute shader;
`m_depth_image` for scene depth.

Engine::initCommands(): Two kinds of command pool and command buffer.
Per-frame command pool and buffer are in each frame data structure, for drawing.
Immediate command pool and buffer are engine members, used for one-time commands like data upload.

Engine::initSyncStructures():
Per-frame fence and semaphore, for rendering sync. Necessary for double-buffering.
Immediate command fence, for uploading sync.

Engine::initDescriptors():
Descriptor is used to describe data io for shaders.
Des layout describe types of data going to bind with pipeline.
Des set is allocated from des pool by the description of des layout, holding the actual buffer or image.
We have a custom descriptorset allocator with a naive pool management.
Per-frame allocator is for texture and gpu scene data, in graphics pipeline.
Global allocator is currently for custom `m_color_image`, in compute pipeline.

Engine::initPipelines():
Compute pipeline layout = descriptorset layout + push constants range.
Compute pipeline has one shader stage only, its module is where to attach the shader module.
Compute pipeline = shader stage + pipeline layout.
Graphics pipeline layout needs the same.
There are many other config for graphics pipeline creation. We have a builder for this.

Engine::initImGui():
Not sure about the whole creation since it's behind current ImGui version.
Further exploration needed.

Engine::initDefaultData():
Example of data loading, binding and uploading to GPU.
Image and sampler.
Mesh, material and texture from GlTF.

## Engine::run()

Event handling, overall configure, UI updating and pipeline draw.

Should this be a delta-time based tick?

## Engine::draw()

Called by Engine::run().

## Engine::cleanup()

## Extern lib

SDL, ImGui

## Vulkan


## Custom data

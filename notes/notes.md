# Issues

ImGui still needs dynamic rendering now?

How we pass data to and from shader?

Scene structure, material model and shading model are all to be explored.

Render pass and dynamic rendering.

# TODO

## Function

- [x] detailed compute pipeline for data processing. See branch `var.compute`
- [ ] deferred rendering.
- [ ] Reversed-z: projection mat, depth compare operator, depth attachment clear value.
- [ ] GPU-driven, pbr ibl, cascaded shadow mapping.
- [ ] CVAR system for configurable renderer.
- [ ] move to RenderPass.
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


# System Procedure

## Engine::init()

Items to init:
- SDL
- Vulkan API
- swapchain
- command pools and buffers
- sync structures
- descriptors for shader IO
- pipelines
- ImGUI
- scene (mesh and data)
- camera

## Engine::run()

```cpp
while (not quitting) {
  poolEvents();
  updateGUI();
  draw();
}
```

## Engine::cleanup()

First wait device to idle.

Items to clean:
- loaded scenes
- frame-dedicated data and deletion queue
- material resources (pipelines)
- main deletion queue
- ImGUI and SDL

## Engine::initDefaultData()

- Images for default colors, currently stored in Engine class.
- Default samplers for images, linear and nearest.
- A scene from .glb file, need to be refactored.

## Engine::draw()

update scene
wait and get target frame
fill command buffer
submit command buffer, present to swapchain

## Engine::updateScene()

update camera to get view matrix.

clear draw context of last frame, and fill it with current scene

upload scene data: view matrix, ambient color, light source.

# Data Stream

## Scene Data

```cpp
std::optional<std::shared_ptr<LoadedGLTF>>
loadGltf(Engine *engine, std::filesystem::path file_path);
```

material is currently held by Engine::m_metal_rough_mat and responsible for creating material instances (pipeline, desc set, pass type).



## Vulkan Data

auxiliary data: global data, frame-dedicated data

---

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

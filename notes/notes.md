# Issues

ImGui still needs dynamic rendering now?

How we pass data to and from shader?

Scene structure, material model and shading model are all to be explored.

Render pass and dynamic rendering.

# TODO

## Function

- [x] detailed compute pipeline for data processing. See branch `var.compute`
- [x] move to RenderPass.
- [ ] vulkan backend and RAII.
- [ ] deferred rendering.
- [ ] Debug and stat.
- [ ] ECS scene.
- [ ] RDG
- [ ] GPU-driven, pbr ibl, other global features
- [ ] Multithreading.
- [ ] Eigen?

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

# pipelines and render pass

Actual binding:
- `VkRenderPass`, `VkFrameBuffer` to begin a render pass
- `VkPipeline` to bind a pipeline, `VkPipelineLayout` to set pipeline IO.

## Build a render pass

## Build a pipeline

`VkGraphicsPipelineCreateInfo`, typical:
- shader stage create infos, where `VkShaderModule` is filled.
- vertex input state
  - relies on scene data structure
  - can be empty if vertices are sent using VBA


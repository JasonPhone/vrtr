# Issues

ImGui still needs dynamic rendering now?

How we pass data to and from shader?

Scene structure, material model and shading model are all to be explored.

Render pass and dynamic rendering.

# TODO

## Function

- [x] detailed compute pipeline for data processing
  - [x] Pure compute pipeline.
  - [x] Shader IO with format all FP32.
- [x] data processing structure

## Compute shader constants

gl_WorkGroupSize:
- (local_size_x, local_size_y, local_size_z).
- Size of a tile, xyz.

gl_NumWorkGroups:
- Params in vkCmdDispatch().
- Number of tiles, xyz.

gl_WorkGroupID:
- [0, gl_NumWorkGroups).
- Index of a tile.

gl_LocalInvocationID:
- [0, gl_WorkGroupSize).
- Element index relative to current tile, xyz.

gl_GlobalInvocationID:
- gl_WorkGroupID * gl_WorkGroupSize + gl_LocalInvocationID
- Absolute element index.

gl_LocalInvocationIndex:
- gl_LocalInvocationID.z * gl_WorkGroupSize.x * gl_WorkGroupSize.y + gl_LocalInvocationID.y * gl_WorkGroupSize.x + gl_LocalInvocationID.x
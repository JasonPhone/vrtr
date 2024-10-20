描述：一个实时渲染引擎，用于各种渲染算法的快速验证。

细节：
- 计算加速基于 Vulkan 实现
  - Use RenderPass
- 重点是延迟管线
- 子系统（优先级）
  - mesh，材质
  - 动画
  - 粒子
  - 物理
- 移动端？filament

# Engine Clock

tick() receives arbitrary delta t.

separate timers for render simulator and camera.

```cpp
begin_tick = timer().now();
while (true) {
  poolEvents();
  scene.tick(scene_delta);
  camera.tick(camera_delta);
  /// Other systems tick().
  render();
  end_tick = timer().now();
  /// Use this for simulation synced with reality.
  real_delta = end_tick - begin_tick;
  begin_tick = end_tick;
}
```


# Managers


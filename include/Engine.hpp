#pragma once

#include "GPU/GPU.hpp"
#include "Scene/Scene.hpp"
#include "utils/json.hpp"
#include "Window.hpp"

namespace vrtr {
class Engine {
public:
  void init(const Json &config, const Window *window) {
    m_window = window;
    m_gpu.init(m_window->getSDLHandle());
    m_gpu.uploadScene(m_scene);
  }
  void deinit() { m_gpu.deinit(); }
  void tick(float delta) { m_scene.tick(delta); }
  void draw() {
    m_gpu.updateScene(m_scene);
    m_gpu.draw();
  }

private:
  GPU m_gpu;
  Scene m_scene;
  const Window *m_window;
};
} // namespace vrtr
#pragma once
#include "vk_types.h"
#include <SDL3/SDL_events.h>

class Camera {
public:
  glm::vec3 position;
  glm::vec3 velocity;
  float pitch = 0.f, yaw = 0.f;
  bool enabled = false;

  glm::mat4 getViewMatrix();
  glm::mat4 getRotationMatrix();
  void processSDLEvent(SDL_Event &e);
  void update();
  void init();
};
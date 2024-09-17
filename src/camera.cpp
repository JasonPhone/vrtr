#include "camera.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

void Camera::update() {
  glm::mat4 camera_rotation = getRotationMatrix();
  position += glm::vec3(camera_rotation * glm::vec4(velocity * 0.2f, 0.f));
}

void Camera::processSDLEvent(SDL_Event &e) {
  if (e.type == SDL_EVENT_KEY_DOWN) {
    switch (e.key.key) {
    case SDLK_W:
      velocity.z = -1;
      break;
    case SDLK_S:
      velocity.z = 1;
      break;
    case SDLK_A:
      velocity.x = -1;
      break;
    case SDLK_D:
      velocity.x = 1;
      break;
    case SDLK_Q:
      velocity.y = 1;
      break;
    case SDLK_E:
      velocity.y = -1;
      break;
    }
  }
  if (e.type == SDL_EVENT_KEY_UP) {
    switch (e.key.key) {
    case SDLK_W:
      velocity.z = 0;
      break;
    case SDLK_S:
      velocity.z = 0;
      break;
    case SDLK_A:
      velocity.x = 0;
      break;
    case SDLK_D:
      velocity.x = 0;
      break;
    case SDLK_Q:
      velocity.y = 0;
      break;
    case SDLK_E:
      velocity.y = 0;
      break;
    }
  }
  if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    enabled = true;
  if (e.type == SDL_EVENT_MOUSE_BUTTON_UP)
    enabled = false;
  if (enabled && e.type == SDL_EVENT_MOUSE_MOTION) {
    yaw += (float)e.motion.xrel / 300.f;
    pitch -= (float)e.motion.yrel / 300.f;
  }
}
glm::mat4 Camera::getViewMatrix() {
  // Applied on the world, so inverse it.
  glm::mat4 rotation = getRotationMatrix();
  glm::mat4 translation = glm::translate(glm::mat4(1.f), position);
  return glm::inverse(translation * rotation);
}
glm::mat4 Camera::getRotationMatrix() {
  // FPS style camera.
  glm::quat pitch_rotation = glm::angleAxis(pitch, glm::vec3{1.f, 0.f, 0.f});
  glm::quat yaw_rotation = glm::angleAxis(yaw, glm::vec3{0.f, -1.f, 0.f});

  return glm::toMat4(yaw_rotation) * glm::toMat4(pitch_rotation);
}
void Camera::init() {
  position = glm::vec3{0, 0, 5};
  velocity = glm::vec3{0.f};
  pitch = yaw = 0.f;
  enabled = false;
}
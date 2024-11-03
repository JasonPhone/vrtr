#pragma once
#include "utils/vk/common.hpp"
#include <array>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

namespace vrtr {
struct Vertex {
  glm::vec3 position;
  glm::vec3 color;
  glm::vec2 tex_coord;
  /// NOTE Vertex input binding and attribute descriptions are needed
  /// to create graphic pipeline, irrelevant with scene content.
  static VkVertexInputBindingDescription getVertexBindingDescription() {
    VkVertexInputBindingDescription binding_description = {};
    binding_description.binding = 0;
    binding_description.stride = sizeof(Vertex);
    // Goto next entry after each vertex.
    binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return binding_description;
  }
  static std::array<VkVertexInputAttributeDescription, 3>
  getVertexAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, position);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, tex_coord);

    return attributeDescriptions;
  }
};

struct SceneData {
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
  alignas(16) glm::mat4 model;
  // vec4 ambientColor;
  // vec4 sunlightDirection; //w for sun power
  // vec4 sunlightColor;
};

class Scene {
public:
  Scene() {
    m_vertices = {{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                  {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                  {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                  {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

                  {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                  {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                  {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                  {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}};

    m_indices = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};
  }
  const std::vector<Vertex> &getVertices() const { return m_vertices; }
  const std::vector<uint32_t> &getIndices() const { return m_indices; }
  SceneData getSceneData() const { return m_scene_data; }

  void tick(float delta) {
    m_time += delta;
    m_scene_data.model =
        glm::rotate(glm::mat4(1.f), m_time * 0.1f * glm::radians(1.f),
                    glm::vec3(0.f, 0.f, 1.f));
    m_scene_data.view =
        glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                    glm::vec3(0.0f, 0.0f, 1.0f));
    m_scene_data.proj =
        glm::perspective(glm::radians(45.0f), 1920.f / 1080, 0.1f, 1000.0f);
    m_scene_data.proj[1][1] *= -1;
  }

private:
  float m_time = 0;
  std::vector<Vertex> m_vertices;
  std::vector<uint32_t> m_indices;
  SceneData m_scene_data;
};
} // namespace vrtr
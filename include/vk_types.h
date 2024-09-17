/**
 * @file vk_types.h
 * @author ja50n (zs_feng@qq.com)
 * @brief Base types and utils for this project.
 * @version 0.1
 * @date 2024-08-13
 */
#pragma once

#include <array>
#include <deque>
#include <stack>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-extension"
#pragma clang diagnostic ignored "-Wnullability-completeness"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#include <vk_mem_alloc.h>
#pragma clang diagnostic pop

#include <fmt/core.h>

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#define VK_CHECK(x)                                                            \
  do {                                                                         \
    VkResult err = x;                                                          \
    if (err) {                                                                 \
      fmt::print("Detected Vulkan error: {}, {}:{}", string_VkResult(err),     \
                 __FILE__, __LINE__);                                          \
      abort();                                                                 \
    }                                                                          \
  } while (0)

#define VK_ONE_SEC 1000000000

class Engine;
/**
 * @brief Base class.
 */
class ObjectBase {};

/**
 * @brief Separate image.
 *        Images from swapchain are not guaranteed in formats
 *        (may be low precision) and have fixed resolution only.
 */
struct AllocatedImage {
  VkImage image;
  VkImageView view;
  VkExtent3D extent;
  VkFormat format;

  VmaAllocation allocation;
};
/**
 * @brief Push data to shader using 'Buffer Device Address'.
 */
struct AllocatedBuffer {
  VkBuffer buffer;

  VmaAllocation allocation;
  VmaAllocationInfo alloc_info;
};
struct Vertex {
  glm::vec3 position;
  float uv_x;
  glm::vec3 normal;
  float uv_y;
  glm::vec4 color;
};
struct GPUMeshBuffers {
  AllocatedBuffer index_buffer;
  AllocatedBuffer vertex_buffer;
  VkDeviceAddress vertex_buffer_address;
};
struct GPUDrawPushConstants {
  glm::mat4 world_mat;
  VkDeviceAddress vertex_buffer_address;
};

struct MaterialPipeline {
  VkPipeline pipeline;
  VkPipelineLayout layout;
};
enum class MaterialPass : uint8_t { BasicMainColor, BasicTransparent, Others };
/**
 * @brief Final material instance, ready to render,
 *        holding the actual shading pipeline and binding infomation.
 */
struct MaterialInstance {
  MaterialPipeline *p_pipeline;
  VkDescriptorSet ds;
  MaterialPass pass_type;
};
struct GLTFMaterial {
  MaterialInstance data;
};

struct GeometryBound {
  glm::vec3 origin;
  float radius;
};
struct GeometrySurface {
  uint32_t start_index;
  uint32_t count;
  std::shared_ptr<GLTFMaterial> material;
  GeometryBound bound;
};
struct MeshAsset {
  std::string name;
  std::vector<GeometrySurface> surfaces;
  GPUMeshBuffers mesh_buffers;
};
#pragma once
#include "vk_types.h"
#include "vk_descriptors.h"

/**
 * @brief Render-ready data, last stage from CPU to GPU. Holds
 *        - mesh vertex (by device address) and vertex index.
 *        - world transform matrix.
 *        - material for this mesh.
 */
struct RenderObject {
  uint32_t n_index;
  uint32_t first_index;
  VkBuffer index_buffer;
  MaterialInstance *material;
  glm::mat4 transform;
  VkDeviceAddress vertex_buffer_address;
  GeometryBound bound;
};
struct DrawContext {
  std::vector<RenderObject> opaque_surfaces;
  std::vector<RenderObject> transparent_surfaces;
};

/// @brief Interface for everything that can be rendered.
struct IRenderable {
  virtual ~IRenderable() {}
  /**
   * @brief Convert the data into render-ready form.
   * @note  Not drawing to screen, just "draw" to the context,
   *        will be submit to Vulkan later.
   *
   * @param top_matrix  Temp transform matrix
   *                    applied to this object and its children,
   * @param context Output.
   */
  virtual void draw(const glm::mat4 &top_matrix, DrawContext &context) = 0;
};

/// @brief Basic brick of a scene, supposed to be part of a  tree structure.
struct Node : public IRenderable {
  std::weak_ptr<Node> parent; // Avoid circular dependence.
  std::vector<std::shared_ptr<Node>> children;
  glm::mat4 transform_local; // Transform directly from model data.
  glm::mat4 transform_world; // Applied transform from us.

  void updateTransform(const glm::mat4 &parent_matrix);

  virtual void draw(const glm::mat4 &top_matrix, DrawContext &context) override;
  virtual ~Node() {}
};
struct MeshNode : public Node {
  std::shared_ptr<MeshAsset> mesh;
  virtual void draw(const glm::mat4 &top_matrix, DrawContext &context) override;
};
struct LoadedGLTF : public IRenderable {
  // Storage all the data on a given glTF file.
  std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
  std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
  std::unordered_map<std::string, AllocatedImage> images;
  std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;
  // Nodes having no parent, for iterating through the file in tree order.
  std::vector<std::shared_ptr<Node>> top_nodes;

  std::vector<VkSampler> samplers;
  DescriptorAllocator descriptor_pool;
  AllocatedBuffer material_data_buffer;
  Engine *creator;

  ~LoadedGLTF() { clearAll(); };

  virtual void draw(const glm::mat4 &top_mat, DrawContext &context) override;

private:
  void clearAll();
};
#include "renderable.h"
#include "vk_engine.h"

void Node::updateTransform(const glm::mat4 &parent_matrix) {
  transform_world = parent_matrix * transform_local;
  for (auto c : children) {
    c->updateTransform(transform_world);
  }
}
void Node::draw(const glm::mat4 &top_matrix, DrawContext &context) {
  for (const auto &c : children)
    c->draw(top_matrix, context);
}

void MeshNode::draw(const glm::mat4 &top_matrix, DrawContext &context) {
  glm::mat4 node_matrix = top_matrix * transform_world;
  for (auto &s : mesh->surfaces) {
    RenderObject surface;
    surface.first_index = s.start_index;
    surface.n_index = s.count;
    surface.index_buffer = mesh->mesh_buffers.index_buffer.buffer;
    surface.material = &s.material->data;
    surface.transform = node_matrix;
    surface.vertex_buffer_address = mesh->mesh_buffers.vertex_buffer_address;

    surface.bound = s.bound;

    switch (s.material->data.pass_type) {
    case MaterialPass::BasicMainColor:
      context.opaque_surfaces.push_back(surface);
    case MaterialPass::Others:
      break;
    case MaterialPass::BasicTransparent:
      // context.transparent_surfaces.push_back(surface);
      break;
    }
  }
  Node::draw(top_matrix, context);
}

void LoadedGLTF::draw(const glm::mat4 &top_mat, DrawContext &context) {
  for (auto &n : top_nodes)
    n->draw(top_mat, context);
}
void LoadedGLTF::clearAll() {
  VkDevice dv = creator->m_device;
  descriptor_pool.destroyPools(dv);
  creator->destroyBuffer(material_constants_buffer);

  for (auto &[k, v] : meshes) {
    creator->destroyBuffer(v->mesh_buffers.index_buffer);
    creator->destroyBuffer(v->mesh_buffers.vertex_buffer);
  }
  for (auto &[k, v] : images) {
    if (v.image == creator->m_error_image.image) {
      // dont destroy the default images
      continue;
    }
    creator->destroyImage(v);
  }
  for (auto &sampler : samplers)
    vkDestroySampler(dv, sampler, nullptr);
}
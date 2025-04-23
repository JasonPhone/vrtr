
#pragma once
#include <string_view>
#include <vector>
#include <memory>
#include <functional>
#include <utils/vk/buffers.hpp>
#include <utils/vk/images.hpp>
#include "utils/log.hpp"
#include "utils/vk/allocation.hpp"

namespace rdg {

class Node {
public:
  enum class NodeType { Pass, Resource };
  Node(std::string_view name, NodeType type)
      : m_name(name), m_node_type(type) {}

  std::string_view m_name;
  uint32_t m_in_ref = 0;
  uint32_t m_out_ref = 0;
  NodeType m_node_type;
};

struct Edge {
public:
  enum class AccessType : uint8_t {
    Unknown = 0,
    ReadOnly = 1 << 0,
    WriteOnly = 1 << 1,
    ReadWrite = ReadOnly | WriteOnly
  };
  Edge(std::shared_ptr<Node> from, std::shared_ptr<Node> to, AccessType access)
      : from(from), to(to), access(access) {}

  std::shared_ptr<Node> from;
  std::shared_ptr<Node> to;
  AccessType access = AccessType::Unknown;
};

class RenderGraph;
class PassNode;
class ResourceNode;

struct PassContext {
  RenderGraph &graph;
  VkCommandBuffer &cmd_buffer;
  VkDevice &device;
  VmaAllocator &allocator;
  VkExtent3D render_extent;
};

class ResourceNode : public Node {
public:
  enum class ResourceType { Image, Buffer, Others };
  virtual void allocateResource(VkDevice &device, VmaAllocator &allocator) = 0;
  virtual ~ResourceNode() {}

  ResourceNode(std::string_view name, ResourceType type)
      : Node(name, Node::NodeType::Resource), m_resource_type(type) {}
  ResourceType m_resource_type;
  bool m_allocated = false;
  bool m_external = false;

private:
};

/// NOTE More abstraction?
class ImageNode : public ResourceNode {
public:
  struct ImageDescription {
    VkImageUsageFlags usage;
    VkExtent3D extent;
    VkFormat format;
    bool mipmap;
  };
  ImageNode(std::string_view name) : ResourceNode(name, ResourceType::Image) {}
  void setImageDescription(ImageDescription &desc) { m_description = desc; }
  void allocateResource(VkDevice &device, VmaAllocator &allocator) override {
    if (m_allocated)
      return;
    // vkimage::ImageBuilder builder;
    // m_image = builder.setUsage(m_description.usage)
    //               .setExtent(m_description.extent)
    //               .setFormat(m_description.format)
    //               .build(device, allocator, m_description.mipmap);
    LOGD("Allocate image {}", m_name);
    m_allocated = true;
    m_external = false;
  }
  void importResource(AllocatedImage &image) {
    m_image = image;
    m_allocated = true;
    m_external = true;
  }
  ImageDescription m_description;
  AllocatedImage m_image;
};

class BufferNode : public ResourceNode {
public:
  struct BufferDescription {
    VkBufferUsageFlags buffer_usage;
    VmaMemoryUsage memory_usage;
    uint32_t size;
  };
  BufferNode(std::string_view name)
      : ResourceNode(name, ResourceType::Buffer) {}

  void setBufferDescription(BufferDescription &desc) { m_description = desc; }
  void allocateResource(VkDevice &device, VmaAllocator &allocator) override {
    if (m_allocated)
      return;
    // vkbuffer::BufferBuilder builder;
    // m_buffer = builder.setBufferUsage(m_description.buffer_usage)
    //                .setMemoryUsage(m_description.memory_usage)
    //                .setSize(m_description.size)
    //                .build(allocator);
    LOGD("Allocate buffer {}", m_name);
    m_allocated = true;
    m_external = false;
  }
  void importResource(AllocatedBuffer &buffer) {
    m_buffer = buffer;
    m_allocated = true;
    m_external = true;
  }
  BufferDescription m_description;
  AllocatedBuffer m_buffer;
};

class PassNode : public Node {
public:
  using PassCallback = std::function<void(PassContext &)>;
  PassNode(std::string_view name,
           std::vector<std::shared_ptr<ResourceNode>> &input,
           std::vector<std::shared_ptr<ResourceNode>> &output, PassCallback &cb)
      : Node(name, Node::NodeType::Pass), m_pass_input(input),
        m_pass_output(output), m_execute(cb) {
    // m_in_ref = input.size();
    // m_out_ref = output.size();
  }
  void setInput(std::vector<std::shared_ptr<ResourceNode>> &input) {
    m_pass_input = input;
    // m_in_ref = input.size();
  }
  void setOutput(std::vector<std::shared_ptr<ResourceNode>> &output) {
    m_pass_output = output;
    // m_out_ref = output.size();
  }
  void execute(PassContext &context) { m_execute(context); }

  std::vector<std::shared_ptr<ResourceNode>> m_pass_input;
  std::vector<std::shared_ptr<ResourceNode>> m_pass_output;
  PassCallback m_execute;

private:
};

} // namespace rdg
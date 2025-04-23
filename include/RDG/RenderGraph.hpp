#pragma once
#include <vulkan/vulkan.h>
#include <queue>
#include <stack>
#include "RDG/graph_nodes.hpp"

namespace rdg {
class RenderGraph {
public:
  void addPass(std::string_view name,
               std::vector<std::shared_ptr<ResourceNode>> &&input,
               std::vector<std::shared_ptr<ResourceNode>> &&output,
               PassNode::PassCallback &&cb) {
    LOGD("RDG add pass {}, {} input, {} output", name, input.size(),
         output.size());
    auto pass_node =
        std::make_shared<PassNode>(PassNode{name, input, output, cb});
    m_pass_nodes[name] = pass_node;
    /// TODO Nodes may be reused, no matter PassNode or ResourceNode.
    for (auto &input_node : input) {
      m_resource_nodes[input_node->m_name] = input_node;

      auto from_node = std::static_pointer_cast<Node, ResourceNode>(input_node);
      auto to_node = std::static_pointer_cast<Node, PassNode>(pass_node);
      auto edge = std::make_shared<Edge>(
          Edge(from_node, to_node, Edge::AccessType::ReadWrite));
      m_in_edges[to_node].push_back(edge);
      m_out_edges[from_node].push_back(edge);
      from_node->m_out_ref += 1;
    }
    for (auto &output_node : output) {
      m_resource_nodes[output_node->m_name] = output_node;

      auto from_node = std::static_pointer_cast<Node, PassNode>(pass_node);
      auto to_node = std::static_pointer_cast<Node, ResourceNode>(output_node);
      auto edge = std::make_shared<Edge>(
          Edge(from_node, to_node, Edge::AccessType::ReadWrite));
      m_in_edges[to_node].push_back(edge);
      m_out_edges[from_node].push_back(edge);
      from_node->m_out_ref += 1;
    }
  }
  void compile(VkDevice &device, VmaAllocator &allocator) {
    LOGD("RDG compile");
    for (auto &node : m_resource_nodes) {
      node.second->allocateResource(device, allocator);
    }
  }
  void execute(PassContext &context) {
    LOGD("RDG sort");
    std::queue<std::shared_ptr<Node>> que;
    std::stack<std::shared_ptr<PassNode>> passes;
    for (auto &node : m_resource_nodes) {
      if (node.second->m_out_ref == 0) {
        que.push(std::static_pointer_cast<ResourceNode, Node>(node.second));
        LOGD("start from {}", node.second->m_name);
      }
    }

    while (!que.empty()) {
      auto node = que.front();
      que.pop();

      LOGD("at node {}", node->m_name);

      if (node->m_node_type == Node::NodeType::Pass) {
        passes.push(std::static_pointer_cast<PassNode, Node>(node));
        LOGD("pass {} staged", node->m_name);
      }

      auto &in_edges = m_in_edges[node];
      for (auto &edge : in_edges) {
        edge->from->m_out_ref -= 1;
        LOGD("node {} decrease ref, now {}", edge->from->m_name,
             edge->from->m_out_ref);
        if (edge->from->m_out_ref == 0) {
          que.push(edge->from);
          LOGD("node {} enqueued", edge->from->m_name);
        }
      }
    }

    LOGD("RDG execute");
    while (!passes.empty()) {
      auto &pass = passes.top();
      LOGD("execute pass {}", pass->m_name);
      passes.pop();
      pass->execute(context);
    }
  }

  std::unordered_map<std::string_view, std::shared_ptr<PassNode>> m_pass_nodes;
  std::unordered_map<std::string_view, std::shared_ptr<ResourceNode>>
      m_resource_nodes;
  std::unordered_map<std::string_view, std::shared_ptr<Node>> m_nodes;
  std::unordered_map<std::shared_ptr<Node>, std::vector<std::shared_ptr<Edge>>>
      m_out_edges;
  std::unordered_map<std::shared_ptr<Node>, std::vector<std::shared_ptr<Edge>>>
      m_in_edges;
};
} // namespace rdg
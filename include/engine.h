#pragma once
#include "vk_types.h"
#include "vk_descriptors.h"

/**
 * @brief Manage the deletion.
 * @note Better store vulkan handles instead of functions.
 */
struct DeletionQueue {
  std::stack<std::function<void()>> delete_callbacks;
  void push(std::function<void()> &&function) {
    delete_callbacks.push(function);
  }
  void flush() {
    while (!delete_callbacks.empty()) {
      delete_callbacks.top()();
      delete_callbacks.pop();
    }
  }
};

struct ComputePushConstants {
  glm::vec4 data1;
  glm::vec4 data2;
  glm::vec4 data3;
  glm::vec4 data4;
};

struct ComputePipeline {
  const char *name;
  VkPipeline pipeline;
  VkPipelineLayout layout;
  ComputePushConstants data;
};

struct ComputeTask {
  /**
   * Input: all FP32 data.
   *  path to data
   *  ready for memcpy and buffer upload.
   * Output: all FP32 data.
   *  path, name, index.
   *
   */
  std::string input_0;
  std::string input_1;
  std::string input_2;
  std::string output_0;
};

struct Timer {
  float period_ms;
  std::chrono::time_point<std::chrono::system_clock> start_point;
  void begin() { start_point = std::chrono::system_clock::now(); }
  void end() {
    auto end_point = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        end_point - start_point);
    period_ms = elapsed.count() / 1000.f;
  }
};

struct EngineStats {
  int n_triangles;
  int n_drawcalls;
  Timer t_frame;
  Timer t_scene_update;
  Timer t_cpu_draw;
};

constexpr size_t kNInputBuffers = 3;
constexpr size_t kNOutputBuffers = 1;

constexpr float kWidth = 1280, kHeight = 720;
constexpr size_t kBufferSize = 1280 * 720 * 4 * sizeof(float);

class Engine : public ObjectBase {
public:
  // Engine() = delete;
  void init();
  void run();
  void process();
  void cleanup();

  bool is_initialized{false};
  int task_number{0};
  EngineStats stats;

  static Engine &get();

private:
  VkInstance m_instance;                  // Vulkan library handle
  VkDebugUtilsMessengerEXT m_debug_msngr; // Vulkan debug output handle
  VkPhysicalDevice m_chosen_GPU;          // GPU chosen as the default device
  VkDevice m_device;                      // Vulkan device for commands

  VkExtent2D m_draw_extent;

  VkQueue m_compute_queue;
  uint32_t m_compute_queue_family;

  DeletionQueue m_main_deletion_queue;
  VmaAllocator m_allocator;

  // Input.
  std::vector<AllocatedBuffer> m_input_buffers;
  VkDescriptorSet m_input_ds;
  VkDescriptorSetLayout m_input_ds_layout;
  // Output.
  std::vector<AllocatedBuffer> m_output_buffers;
  VkDescriptorSet m_output_ds;
  VkDescriptorSetLayout m_output_ds_layout;
  std::vector<AllocatedBuffer> m_readback_buffers;

  DescriptorAllocator m_global_ds_allocator;

  VkPipelineLayout m_compute_layout;
  ComputePipeline m_compute_pipeline;

  VkFence m_compute_fence;
  VkCommandBuffer m_compute_cmd;
  VkCommandPool m_compute_cmd_pool;
  VkFence m_imm_fence;
  VkCommandBuffer m_imm_cmd;
  VkCommandPool m_imm_cmd_pool;

  VkQueryPool m_query_pool_timestamp;
  std::vector<uint64_t> m_timestamps;
  float m_timestamp_period;
  std::vector<ComputeTask> m_tasks;

private:
  void initVulkan();
  void initBuffers();
  void initCommands();
  void initSyncStructures();

  void initDescriptors();
  void initPipelines();
  void initComputePipelines();
  void initDefaultData();

  ComputeTask &getCurrentTask() { return m_tasks[task_number]; }

  void updateInput();
  void doCompute(VkCommandBuffer cmd);
  void getResult(VkCommandBuffer cmd);

  AllocatedBuffer createBuffer(
      size_t alloc_size, VkBufferUsageFlags usage, VmaMemoryUsage mem_usage,
      VmaAllocationCreateFlagBits mem_alloc = VMA_ALLOCATION_CREATE_MAPPED_BIT);
  void destroyBuffer(const AllocatedBuffer &buffer);

  void immediateSubmit(std::function<void(VkCommandBuffer cmd)> &&func);
};

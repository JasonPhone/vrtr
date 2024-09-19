#pragma once
#include "vk_types.h"
#include "vk_descriptors.h"
#include "camera.h"

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

// FIXME This affects imgui drag lagging.
constexpr uint32_t kFrameOverlap = 3;

class Engine : public ObjectBase {
public:
  // Engine() = delete;
  void init();
  void run();
  void draw();
  void cleanup();
  void immediateSubmit(std::function<void(VkCommandBuffer cmd)> &&func);
  GPUMeshBuffers uploadMesh(std::span<uint32_t> indices,
                            std::span<Vertex> vertices);

  bool stop_rendering{false};
  bool require_resize{false};
  bool is_initialized{false};
  int frame_number{0};
  VkExtent2D window_extent{1920, 1080};
  EngineStats stats;

  static Engine &get();

  /// @brief Create GPU-only image.
  AllocatedImage createImage(VkExtent3D size, VkFormat format,
                             VkImageUsageFlags usage, bool mipmap = false);
  /// @brief Create GPU-only image with data.
  AllocatedImage createImage(void *data, VkExtent3D size, VkFormat format,
                             VkImageUsageFlags usage, bool mipmap = false);
  void destroyImage(const AllocatedImage &image);

private:
  // TODO Better visibility.
  VkInstance m_instance;                  // Vulkan library handle
  VkDebugUtilsMessengerEXT m_debug_msngr; // Vulkan debug output handle
  VkPhysicalDevice m_chosen_GPU;          // GPU chosen as the default device
  VkDevice m_device;                      // Vulkan device for commands
  VkSurfaceKHR m_surface;                 // Vulkan window surface

  VkExtent2D m_draw_extent;
  float m_render_scale = 1.f;

  VkDescriptorSetLayout m_GPU_scene_data_ds_layout;
  VkQueue m_graphic_queue;
  uint32_t m_graphic_queue_family;

  DeletionQueue m_main_deletion_queue;

  VmaAllocator m_allocator;

  // Input images.
  AllocatedImage m_white_image;
  AllocatedImage m_black_image;
  AllocatedImage m_gray_image;
  AllocatedImage m_error_image;
  VkSampler m_default_sampler_linear;
  VkSampler m_default_sampler_nearest;
  VkDescriptorSetLayout m_single_image_ds_layout;
  // Output images.
  AllocatedImage m_color_image;
  AllocatedImage m_depth_image;
  VkImage m_save_image;
  VkDeviceMemory m_save_mem;

  DescriptorAllocator m_global_ds_allocator;
  VkDescriptorSet m_draw_image_ds;
  VkDescriptorSetLayout m_draw_image_ds_layout;

  VkPipelineLayout m_compute_pipeline_layout;
  std::vector<ComputePipeline> m_compute_pipelines;
  int m_cur_comp_pipeline_idx = 2;

  void updateScene();
  Camera m_main_camera;

  VkFence m_imm_fence;
  VkCommandBuffer m_imm_cmd;
  VkCommandPool m_imm_cmd_pool;

  VkFence m_compute_fence;
  VkCommandBuffer m_compute_cmd;
  VkCommandPool m_compute_cmd_pool;

  VkQueryPool m_query_pool_timestamp;
  std::vector<uint64_t> m_timestamps;
  float m_timestamp_period;
  bool m_capture = true;

private:
  void initVulkan();
  void initImages();
  void initCommands();
  void initSyncStructures();

  void initDescriptors();
  void initPipelines();
  void initBackgroundPipelines();
  void initDefaultData();

  void drawBackground(VkCommandBuffer cmd);

  AllocatedBuffer createBuffer(size_t alloc_size, VkBufferUsageFlags usage,
                               VmaMemoryUsage mem_usage);
  void destroyBuffer(const AllocatedBuffer &buffer);
};

#pragma once
#include "vk_types.h"
#include "vk_descriptors.h"
#include "vk_loader.h"
#include "renderable.h"
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

struct FrameData {
  VkCommandPool cmd_pool;
  VkCommandBuffer cmd_buffer_main;

  // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
  // Sync between GPU queues, one operation wait another to signal a semaphore.
  VkSemaphore swapchain_semaphore, render_semaphore; // Two one-way channels.
  // Sync between CPU and GPU, CPU waits for some GPU operations to finish.
  VkFence render_fence;

  DeletionQueue deletion_queue;
  DescriptorAllocator frame_descriptors;
};

struct GPUSceneData {
  glm::mat4 view;
  glm::mat4 proj;
  glm::mat4 view_proj;
  glm::mat4 prev_view_proj;
  glm::vec4 ambient_color;
  glm::vec4 sunlight_dir; // w for sun power.
  glm::vec4 sunlight_color;
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

/**
 * @brief Material builder.
 *
 */
struct GLTFMetallicRoughness {
  MaterialPipeline pipeline_opaque;
  MaterialPipeline pipeline_transparent;

  struct MaterialConstants {
    glm::vec4 color_factors;
    glm::vec4 metal_rough_factors;
    // 256 bytes padding.
    glm::vec4 padding[14];
  };
  struct MaterialResources {
    AllocatedImage color_image;
    VkSampler color_sampler;
    AllocatedImage metal_rough_image;
    VkSampler metal_rough_sampler;
    VkBuffer data_buffer;
    uint32_t data_buffer_offset;
  };

  VkDescriptorSetLayout ds_layout;
  DescriptorWriter writer;

  void buildPipelines(Engine *engine);
  void clearResources(VkDevice device);
  MaterialInstance writeMaterial(VkDevice device, MaterialPass pass,
                                 const MaterialResources &resources,
                                 DescriptorAllocator &d_allocator);
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

  struct SDL_Window *window{nullptr};

  static Engine &get();

  FrameData &getCurrentFrame() {
    return m_frames[frame_number % kFrameOverlap];
  }
  /// @brief Create GPU-only image.
  AllocatedImage createImage(VkExtent3D size, VkFormat format,
                             VkImageUsageFlags usage, bool mipmap = false);
  /// @brief Create GPU-only image with data.
  AllocatedImage createImage(void *data, VkExtent3D size, VkFormat format,
                             VkImageUsageFlags usage, bool mipmap = false);
  void destroyImage(const AllocatedImage &image);

private:
  // TODO Better visibility.
  friend struct GLTFMetallicRoughness;
  friend struct LoadedGLTF;
  friend std::optional<std::shared_ptr<LoadedGLTF>>
  loadGltf(Engine *engine, std::filesystem::path file_path);
  VkInstance m_instance;                  // Vulkan library handle
  VkDebugUtilsMessengerEXT m_debug_msngr; // Vulkan debug output handle
  VkPhysicalDevice m_chosen_GPU;          // GPU chosen as the default device
  VkDevice m_device;                      // Vulkan device for commands
  VkSurfaceKHR m_surface;                 // Vulkan window surface

  VkSwapchainKHR m_swapchain;
  VkFormat m_swapchain_img_format;
  std::vector<VkImage> m_swapchain_imgs;
  std::vector<VkImageView> m_swapchain_img_views;
  VkExtent2D m_swapchain_extent;
  VkExtent2D m_draw_extent;
  float m_render_scale = 1.f;

  FrameData m_frames[kFrameOverlap];
  GPUSceneData m_scene_data;
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

  MaterialInstance m_default_material;
  GLTFMetallicRoughness m_metal_rough_mat;

  DescriptorAllocator m_global_ds_allocator;
  VkDescriptorSet m_draw_image_ds;
  VkDescriptorSetLayout m_draw_image_ds_layout;

  VkPipelineLayout m_compute_pipeline_layout;
  std::vector<ComputePipeline> m_compute_pipelines;
  int m_cur_comp_pipeline_idx = 0;
  VkPipelineLayout m_simple_mesh_pipeline_layout;
  VkPipeline m_simple_mesh_pipeline;
  GPUMeshBuffers m_simple_mesh;

  std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> m_loaded_scenes;
  DrawContext m_main_draw_context;
  void updateScene();
  Camera m_main_camera;

  VkFence m_imm_fence;
  VkCommandBuffer m_imm_cmd;
  VkCommandPool m_imm_cmd_pool;

  VkQueryPool m_query_pool_timestamp;
  std::vector<uint64_t> m_timestamps;
  float m_timestamp_period;

private:
  void initVulkan();
  void initSwapchain();
  void initCommands();
  void initSyncStructures();

  void initDescriptors();
  void initPipelines();
  void initBackgroundPipelines();
  void initSimpleMeshPipeline();
  void initDefaultData();

  void initImGui();
  void drawImGui(VkCommandBuffer cmd, VkImageView target_img_view);
  void drawBackground(VkCommandBuffer cmd);
  void drawGeometry(VkCommandBuffer cmd);

  void createSwapchain(int w, int h);
  void resizeSwapchain();
  void destroySwapchain();

  AllocatedBuffer createBuffer(size_t alloc_size, VkBufferUsageFlags usage,
                               VmaMemoryUsage mem_usage);
  void destroyBuffer(const AllocatedBuffer &buffer);
};

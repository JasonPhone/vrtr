#pragma once
#include "utils/DeletionQueue.hpp"
#include "utils/vk/allocation.hpp"
#include "utils/vk/FrameData.hpp"
#include "Scene/Scene.hpp"
#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

namespace vrtr {
constexpr bool kUseValidation = true;
constexpr int kFrameOverlap = 2;

class GPU {
public:
  void init(SDL_Window *window);
  void deinit();
  /// All-time persistent data upload.
  void uploadScene(const Scene &scene);
  /// Frame-dedicated update.
  void updateScene(const Scene &scene);
  AllocatedImage uploadImage(void *data, VkExtent3D size, VkFormat format,
                                   VkImageUsageFlags usage, bool mipmap = false);
  void draw();

private:
  SDL_Window *m_window;
  VkExtent2D m_window_extent;
  void initVulkan();
  VkInstance m_instance;
  VkSurfaceKHR m_surface;
  VkDevice m_device;
  VkPhysicalDevice m_chosen_GPU;
  VkDebugUtilsMessengerEXT m_debug_messenger;
  VkQueue m_graphic_queue;
  int m_graphic_queue_family;
  VmaAllocator m_mem_allocator;

  void initSwapchain();
  void createSwapchain(int w, int h);
  void destroySwapchain();
  VkFormat m_swapchain_format;
  VkExtent2D m_swapchain_extent;
  VkSwapchainKHR m_swapchain;
  std::vector<VkImage> m_swapchain_images;
  std::vector<VkImageView> m_swapchain_image_views;

  void initOffScreenImages();
  AllocatedImage m_color_image;
  AllocatedImage m_depth_image;

  void initCommands();
  void initSyncStructures();
  FrameData &getCurrentFrame() {
    return m_frames[m_frame_number % kFrameOverlap];
  }
  FrameData m_frames[kFrameOverlap];
  size_t m_frame_number = 0;
  VkFence m_imm_fence;
  VkCommandBuffer m_imm_cmd;
  VkCommandPool m_imm_cmd_pool;
  void immediateSubmit(std::function<void(VkCommandBuffer cmd)> &&func);

  void initDescriptors();
  DescriptorAllocator m_descriptor_allocator;
  struct DescriptorSetLayouts {
    VkDescriptorSetLayout scene_data;
  } m_desc_set_layouts;

  void initRenderPass();
  VkRenderPass m_render_pass;
  VkPipelineLayout m_pipeline_layout;
  VkPipeline m_pipeline;
  VkFramebuffer m_framebuffer;

  void initPipelines();
  void initGraphicPipeline();
  AllocatedBuffer m_vertex_buffer;
  // VkDeviceAddress m_vertex_buffer_address;
  AllocatedBuffer m_index_buffer;
  uint32_t m_num_indices;
  SceneData m_scene_data;

  void initFrameBuffers();

  void recordCmdBuffer(VkCommandBuffer cmd);

  void initTextures();
  AllocatedImage m_texture;
  VkSampler m_default_sampler_linear;
  VkSampler m_default_sampler_nearest;

  DeletionQueue m_deletion_queue;
};
} // namespace vrtr
#define VMA_IMPLEMENTATION
#include "engine.h"

#include "vk_initializers.h"
#include "vk_types.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include <VkBootstrap.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

#include <fstream>

constexpr bool bUseValidationLayers = true;

static Engine *loaded_engine = nullptr;

Engine &Engine::get() { return *loaded_engine; }
void Engine::init() {
  // Only one engine initialization is allowed with the application.
  assert(loaded_engine == nullptr);
  loaded_engine = this;

  initVulkan();
  initImages();
  initCommands();
  initSyncStructures();
  initDescriptors();
  initPipelines();

  initDefaultData();
  is_initialized = true;
}

void Engine::cleanup() {
  if (is_initialized) {
    // Order matters, reversed of initialization.
    vkDeviceWaitIdle(m_device); // Wait for GPU to finish.
    m_main_deletion_queue.flush();
  }
  loaded_engine = nullptr;
}

void Engine::draw() {
  fmt::println("draw");
  stats.t_scene_update.begin();
  updateScene();
  stats.t_scene_update.end();
  stats.t_cpu_draw.begin();
  VK_CHECK(vkWaitForFences(m_device, 1, &m_compute_fence, true, VK_ONE_SEC));
  // Free objects dedicated to this frame (in last iteration).
  VK_CHECK(vkResetFences(m_device, 1, &m_compute_fence));

  // Clear the cmd buffer.
  VkCommandBuffer cmd = m_compute_cmd;
  VK_CHECK(vkResetCommandBuffer(cmd, 0));
  // Record cmd. Bit is to tell vulkan buffer is used exactly once.
  VkCommandBufferBeginInfo cmd_begin_info =
      vkinit::cmdBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  m_draw_extent.width = m_color_image.extent.width;
  m_draw_extent.height = m_color_image.extent.height;

  VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));
  vkCmdResetQueryPool(cmd, m_query_pool_timestamp, 0,
                      static_cast<uint32_t>(m_timestamps.size()));
  { // Drawing commands.
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        m_query_pool_timestamp, 0);
    vkutil::transitionImage(cmd, m_color_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_GENERAL);
    drawBackground(cmd);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        m_query_pool_timestamp, 1);

    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        m_query_pool_timestamp, 2);
    vkutil::transitionImage(cmd, m_color_image.image, VK_IMAGE_LAYOUT_GENERAL,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::transitionImage(cmd, m_depth_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        m_query_pool_timestamp, 3);
    // Copy to swapchain.
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        m_query_pool_timestamp, 4);
    vkutil::transitionImage(cmd, m_color_image.image,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        m_query_pool_timestamp, 5);
  }
  VK_CHECK(vkEndCommandBuffer(cmd));

  // Submit commands.
  VkCommandBufferSubmitInfo cmd_submit_info = vkinit::cmdBufferSubmitInfo(cmd);
  VkSubmitInfo2 submit_info =
      vkinit::submitInfo(&cmd_submit_info, nullptr, nullptr);
  VK_CHECK(vkQueueSubmit2(m_graphic_queue, 1, &submit_info, m_compute_fence));

  frame_number++;
  stats.t_cpu_draw.end();
  vkGetQueryPoolResults(m_device, m_query_pool_timestamp, 0,
                        static_cast<uint32_t>(m_timestamps.size()),
                        m_timestamps.size() * sizeof(uint64_t),
                        m_timestamps.data(), sizeof(uint64_t),
                        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

  if (m_capture) {
    m_capture = false;

    immediateSubmit([&](VkCommandBuffer cmd) {
      vkutil::transitionImage(cmd, m_save_image.image,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      vkutil::copyImage(cmd, m_color_image.image, m_save_image.image,
                        m_draw_extent, m_draw_extent);
      vkutil::transitionImage(cmd, m_save_image.image,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_GENERAL);
    });
    uint32_t width = m_draw_extent.width, height = m_draw_extent.height;
    // Get layout of the image (including row pitch)
    VkImageSubresource subr{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
    VkSubresourceLayout subrl;
    vkGetImageSubresourceLayout(m_device, m_save_image.image, &subr, &subrl);
    unsigned char *data;
    vmaMapMemory(m_allocator, m_save_image.allocation, (void **)&data);
    data += subrl.offset;
    fmt::println("offset {}, size {}, arrP {}, depP {}, rowP {}", subrl.offset,
                 subrl.size, subrl.arrayPitch, subrl.depthPitch,
                 subrl.rowPitch);
    std::ofstream out_file("./screen_shot.ppm",
                           std::ios::out | std::ios::binary);
    out_file << "P6\n" << width << "\n" << height << "\n" << 255 << "\n";
    for (uint32_t y = 0; y < height; y++) {
      unsigned int *row = (unsigned int *)data;
      for (uint32_t x = 0; x < width; x++) {
        out_file.write((char *)row, 3);
        row++;
      }
      data += subrl.rowPitch;
    }
    out_file.close();
    fmt::println("Screenshot saved to disk");
    vmaUnmapMemory(m_allocator, m_save_image.allocation);
  }
}
void Engine::drawBackground(VkCommandBuffer cmd) {
  auto &background = m_compute_pipelines[m_cur_comp_pipeline_idx];
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, background.pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                          m_compute_pipeline_layout, 0, 1, &m_draw_image_ds, 0,
                          nullptr);
  vkCmdPushConstants(cmd, m_compute_pipeline_layout,
                     VK_SHADER_STAGE_COMPUTE_BIT, 0,
                     sizeof(ComputePushConstants), &background.data);
  vkCmdDispatch(cmd, std::ceil(m_draw_extent.width / 16.f),
                std::ceil(m_draw_extent.height / 16.f), 1);
}
void Engine::run() {
  bool b_quit = false;
  while (!b_quit) {
    stats.t_frame.begin();
    // Pipeline draw.
    draw();
    b_quit = !m_capture;
    stats.t_frame.end();
  }
}

void Engine::initVulkan() {
  fmt::print("init vulkan\n");
  vkb::InstanceBuilder builder;
  auto inst_ret = builder.set_app_name("Example Vulkan Application")
                      .request_validation_layers(bUseValidationLayers)
                      .use_default_debug_messenger()
                      .require_api_version(1, 3, 0)
                      .build();
  vkb::Instance vkb_inst = inst_ret.value();
  m_instance = vkb_inst.instance;
  m_debug_msngr = vkb_inst.debug_messenger;

  // Vulkan 1.3 features.
  VkPhysicalDeviceVulkan13Features features13{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
  features13.dynamicRendering = true;
  features13.synchronization2 = true;
  // Vulkan 1.2 features.
  VkPhysicalDeviceVulkan12Features features12{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
  features12.bufferDeviceAddress = true;
  features12.descriptorIndexing = true;

  // Select a gpu.
  // We want a gpu that can write to the SDL surface and supports vulkan 1.3
  // with the correct features
  vkb::PhysicalDeviceSelector selector{vkb_inst};
  vkb::PhysicalDevice physical_device = {};
  auto phy_device_list =
      selector.set_minimum_version(1, 3)
          .set_required_features_13(features13)
          .set_required_features_12(features12)
          .require_present(false)
          .add_required_extension(VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME)
          .select_devices();
  // Naive select by physical device property.
  if (phy_device_list.has_value()) {
    for (const auto &d : phy_device_list.value()) {
      if (d.properties.limits.timestampPeriod > 0 &&
          d.properties.limits.timestampComputeAndGraphics) {
        physical_device = d;
        m_timestamp_period = d.properties.limits.timestampPeriod;
        break;
      }
    }
  } else {
    throw std::runtime_error{
        "No suitable physical devices by features and extensions."};
  }
  if (physical_device.physical_device == VK_NULL_HANDLE) {
    throw std::runtime_error{
        "No suitable physical devices by device properties."};
  }

  // Final vulkan device.
  vkb::DeviceBuilder device_builder{physical_device};
  vkb::Device vkb_device = device_builder.build().value();
  // Get the VkDevice handle for the rest of a vulkan application.
  m_device = vkb_device.device;
  m_chosen_GPU = physical_device.physical_device;
  m_graphic_queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
  m_graphic_queue_family =
      vkb_device.get_queue_index(vkb::QueueType::graphics).value();

  // Mem allocator.
  VmaAllocatorCreateInfo ci_alloc = {};
  ci_alloc.physicalDevice = m_chosen_GPU;
  ci_alloc.device = m_device;
  ci_alloc.instance = m_instance;
  ci_alloc.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  vmaCreateAllocator(&ci_alloc, &m_allocator);

  // Profiler.
  m_timestamps.resize(6);
  VkQueryPoolCreateInfo ci_query_pool = {};
  ci_query_pool.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  ci_query_pool.queryType = VK_QUERY_TYPE_TIMESTAMP;
  ci_query_pool.queryCount = static_cast<uint32_t>(m_timestamps.size());
  VK_CHECK(vkCreateQueryPool(m_device, &ci_query_pool, nullptr,
                             &m_query_pool_timestamp));

  m_main_deletion_queue.push([&]() {
    vmaDestroyAllocator(m_allocator);
    vkDestroyQueryPool(m_device, m_query_pool_timestamp, nullptr);
    vkDestroyDevice(m_device, nullptr);
    vkb::destroy_debug_utils_messenger(m_instance, m_debug_msngr);
    vkDestroyInstance(m_instance, nullptr);
  });
}
void Engine::initImages() {
  fmt::print("init images\n");
  // Custom draw image.
  VkExtent3D color_img_ext = {window_extent.width, window_extent.height, 1};
  VkImageUsageFlags color_img_usage = {};
  // Copy from and into.
  color_img_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  color_img_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  // Compute shader writing.
  color_img_usage |= VK_IMAGE_USAGE_STORAGE_BIT;
  // Graphics pipelines draw.
  color_img_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  m_color_image = createImage(color_img_ext, VK_FORMAT_R16G16B16A16_SFLOAT,
                              color_img_usage);

  VkExtent3D depth_img_ext = {window_extent.width, window_extent.height, 1};
  m_depth_image.format = VK_FORMAT_D32_SFLOAT;
  m_depth_image.extent = depth_img_ext;
  VkImageUsageFlags depth_img_usage = {};
  depth_img_usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  m_depth_image =
      createImage(depth_img_ext, VK_FORMAT_D32_SFLOAT, depth_img_usage);

  m_main_deletion_queue.push([&]() {});

  m_save_image = {};
  VkImageCreateInfo ci_image = {.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                .pNext = nullptr};
  ci_image.imageType = VK_IMAGE_TYPE_2D;
  ci_image.format = VK_FORMAT_R8G8B8A8_UNORM;
  // VK_FORMAT_R32G32B32A32_SFLOAT;
  ci_image.extent = color_img_ext;
  ci_image.arrayLayers = 1;
  ci_image.mipLevels = 1;
  ci_image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  ci_image.samples = VK_SAMPLE_COUNT_1_BIT;
  ci_image.tiling = VK_IMAGE_TILING_LINEAR;
  ci_image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  VmaAllocationCreateInfo ci_alloc = {};
  ci_alloc.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
  ci_alloc.requiredFlags =
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
  VK_CHECK(vmaCreateImage(m_allocator, &ci_image, &ci_alloc,
                          &m_save_image.image, &m_save_image.allocation,
                          nullptr));

  m_main_deletion_queue.push([&]() {
    destroyImage(m_color_image);
    destroyImage(m_depth_image);
    // This image has no VkImageView, as shaders don't access it.
    vmaDestroyImage(m_allocator, m_save_image.image, m_save_image.allocation);
  });
}
void Engine::initCommands() {
  fmt::print("init commands\n");
  // Create a command pool for commands submitted to the graphics queue.
  // We also want the pool to allow for resetting of individual command buffers
  VkCommandPoolCreateInfo ci_cmd_pool = vkinit::cmdPoolCreateInfo(
      m_graphic_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  // Init compute cmd.
  VK_CHECK(vkCreateCommandPool(m_device, &ci_cmd_pool, nullptr,
                               &m_compute_cmd_pool));
  VkCommandBufferAllocateInfo cmd_alloc_info =
      vkinit::cmdBufferAllocInfo(m_compute_cmd_pool, 1);
  VK_CHECK(vkAllocateCommandBuffers(m_device, &cmd_alloc_info, &m_compute_cmd));
  // Init immediate cmd.
  VK_CHECK(
      vkCreateCommandPool(m_device, &ci_cmd_pool, nullptr, &m_imm_cmd_pool));
  cmd_alloc_info = vkinit::cmdBufferAllocInfo(m_imm_cmd_pool, 1);
  VK_CHECK(vkAllocateCommandBuffers(m_device, &cmd_alloc_info, &m_imm_cmd));

  m_main_deletion_queue.push([&]() {
    vkDestroyCommandPool(m_device, m_compute_cmd_pool, nullptr);
    vkDestroyCommandPool(m_device, m_imm_cmd_pool, nullptr);
  });
}
void Engine::initSyncStructures() {
  // One fence to control when the gpu has finished rendering the frame.
  // 2 semaphores to synchronize rendering with swapchain.
  fmt::print("init sync structures\n");

  // The fence starts signalled so we can wait on it on the first frame.
  VkFenceCreateInfo ci_fence =
      vkinit::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);

  VK_CHECK(vkCreateFence(m_device, &ci_fence, nullptr, &m_compute_fence));
  VK_CHECK(vkCreateFence(m_device, &ci_fence, nullptr, &m_imm_fence));
  m_main_deletion_queue.push([&]() {
    vkDestroyFence(m_device, m_compute_fence, nullptr);
    vkDestroyFence(m_device, m_imm_fence, nullptr);
  });
}
void Engine::initDescriptors() {
  std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}};
  // Descriptor pool with 10 des sets, 1 image each.
  m_global_ds_allocator.initPool(m_device, 10, sizes);
  {
    DescriptorLayoutBuilder builder;
    builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    m_draw_image_ds_layout =
        builder.build(m_device, VK_SHADER_STAGE_COMPUTE_BIT);
    m_draw_image_ds =
        m_global_ds_allocator.allocate(m_device, m_draw_image_ds_layout);
  }
  {
    DescriptorLayoutBuilder builder;
    builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    m_GPU_scene_data_ds_layout = builder.build(
        m_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  }
  {
    DescriptorLayoutBuilder builder;
    // Combined ds for coupled image and sampler.
    builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    m_single_image_ds_layout =
        builder.build(m_device, VK_SHADER_STAGE_FRAGMENT_BIT);
  }

  DescriptorWriter writer;
  writer.writeImage(0, m_color_image.view, VK_NULL_HANDLE,
                    VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
  writer.updateDescriptorSet(m_device, m_draw_image_ds);

  m_main_deletion_queue.push([&]() {
    m_global_ds_allocator.destroyPools(m_device);
    vkDestroyDescriptorSetLayout(m_device, m_draw_image_ds_layout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_GPU_scene_data_ds_layout, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_single_image_ds_layout, nullptr);
  });
}
void Engine::initPipelines() {
  // Compute pipelines.
  initBackgroundPipelines();
}
void Engine::initBackgroundPipelines() {
  VkPushConstantRange push_range = {};
  push_range.offset = 0;
  push_range.size = sizeof(ComputePushConstants);
  push_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  // Create pipeline layout.
  VkPipelineLayoutCreateInfo ci_comp_layout = {};
  ci_comp_layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  ci_comp_layout.pNext = nullptr;
  ci_comp_layout.pSetLayouts = &m_draw_image_ds_layout;
  ci_comp_layout.setLayoutCount = 1;
  ci_comp_layout.pPushConstantRanges = &push_range;
  ci_comp_layout.pushConstantRangeCount = 1;
  VK_CHECK(vkCreatePipelineLayout(m_device, &ci_comp_layout, nullptr,
                                  &m_compute_pipeline_layout));

  // Common info.
  VkPipelineShaderStageCreateInfo ci_stage = {};
  ci_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  ci_stage.pNext = nullptr;
  ci_stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  ci_stage.pName = "main";
  VkComputePipelineCreateInfo ci_comp_pipeline = {};
  ci_comp_pipeline.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  ci_comp_pipeline.pNext = nullptr;
  ci_comp_pipeline.layout = m_compute_pipeline_layout;
  ci_comp_pipeline.stage = ci_stage; // Copy by values.

  // TODO Make this a func.
  // Fill shader stage info for different shaders.
  VkShaderModule solid_shader;
  if (!vkutil::loadShaderModule("../../assets/shaders/solid.comp.spv", m_device,
                                &solid_shader)) {
    fmt::println("Error building compute shader.");
  }
  ComputePipeline solid;
  solid.layout = m_compute_pipeline_layout;
  solid.name = "solid";
  solid.data = {};
  ci_comp_pipeline.stage.module =
      solid_shader; // Update this stage info, not previous struct.
  VK_CHECK(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1,
                                    &ci_comp_pipeline, nullptr,
                                    &solid.pipeline));
  m_compute_pipelines.push_back(solid);
  vkDestroyShaderModule(m_device, solid_shader, nullptr);

  ComputePipeline gradient;
  VkShaderModule gradient_shader;
  if (!vkutil::loadShaderModule("../../assets/shaders/gradient_color.comp.spv",
                                m_device, &gradient_shader)) {
    fmt::println("Error building compute shader.");
  }
  gradient.layout = m_compute_pipeline_layout;
  gradient.name = "gradient";
  gradient.data = {};
  gradient.data.data1 = glm::vec4{1, 0, 0, 1};
  gradient.data.data2 = glm::vec4{0, 0, 1, 1};
  ci_comp_pipeline.stage.module = gradient_shader;
  VK_CHECK(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1,
                                    &ci_comp_pipeline, nullptr,
                                    &gradient.pipeline));
  m_compute_pipelines.push_back(gradient);
  vkDestroyShaderModule(m_device, gradient_shader, nullptr);

  VkShaderModule grid_shader;
  if (!vkutil::loadShaderModule("../../assets/shaders/grid.comp.spv", m_device,
                                &grid_shader)) {
    fmt::println("Error building compute shader.");
  }
  ComputePipeline grid;
  grid.layout = m_compute_pipeline_layout;
  grid.name = "gradient";
  grid.data = {};
  ci_comp_pipeline.stage.module = grid_shader;
  VK_CHECK(vkCreateComputePipelines(
      m_device, VK_NULL_HANDLE, 1, &ci_comp_pipeline, nullptr, &grid.pipeline));
  m_compute_pipelines.push_back(grid);
  vkDestroyShaderModule(m_device, grid_shader, nullptr);

  VkShaderModule sky_shader;
  if (!vkutil::loadShaderModule("../../assets/shaders/sky.comp.spv", m_device,
                                &sky_shader)) {
    fmt::println("Error building compute shader.");
  }
  ComputePipeline sky;
  sky.layout = m_compute_pipeline_layout;
  sky.name = "sky";
  sky.data = {};
  sky.data.data1 = glm::vec4{0.1, 0.2, 0.4, 0.97};
  ci_comp_pipeline.stage.module = sky_shader;
  VK_CHECK(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1,
                                    &ci_comp_pipeline, nullptr, &sky.pipeline));
  m_compute_pipelines.push_back(sky);
  vkDestroyShaderModule(m_device, sky_shader, nullptr);

  m_main_deletion_queue.push([&]() {
    vkDestroyPipelineLayout(m_device, m_compute_pipeline_layout, nullptr);
    for (auto &&pipeline : m_compute_pipelines) {
      vkDestroyPipeline(m_device, pipeline.pipeline, nullptr);
    }
  });
}

void Engine::immediateSubmit(std::function<void(VkCommandBuffer cmd)> &&func) {
  VK_CHECK(vkResetFences(m_device, 1, &m_imm_fence));
  VK_CHECK(vkResetCommandBuffer(m_imm_cmd, 0));

  VkCommandBuffer cmd = m_imm_cmd;
  VkCommandBufferBeginInfo cmdBeginInfo =
      vkinit::cmdBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  func(cmd);

  VK_CHECK(vkEndCommandBuffer(cmd));
  VkCommandBufferSubmitInfo cmd_info = vkinit::cmdBufferSubmitInfo(cmd);
  VkSubmitInfo2 submit = vkinit::submitInfo(&cmd_info, nullptr, nullptr);

  // Submit command buffer to the queue and execute it.
  // imm_fence will now block until the graphic commands finish execution.
  VK_CHECK(vkQueueSubmit2(m_graphic_queue, 1, &submit, m_imm_fence));
  VK_CHECK(vkWaitForFences(m_device, 1, &m_imm_fence, true, VK_ONE_SEC));
}

AllocatedBuffer Engine::createBuffer(size_t alloc_size,
                                     VkBufferUsageFlags usage,
                                     VmaMemoryUsage mem_usage) {
  VkBufferCreateInfo ci_buffer = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
  };
  ci_buffer.size = alloc_size;
  ci_buffer.usage = usage;
  VmaAllocationCreateInfo ci_alloc = {};
  ci_alloc.usage = mem_usage;
  ci_alloc.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
  AllocatedBuffer buffer;
  VK_CHECK(vmaCreateBuffer(m_allocator, &ci_buffer, &ci_alloc, &buffer.buffer,
                           &buffer.allocation, &buffer.alloc_info));
  return buffer;
}
void Engine::destroyBuffer(const AllocatedBuffer &buffer) {
  vmaDestroyBuffer(m_allocator, buffer.buffer, buffer.allocation);
}

void Engine::initDefaultData() {}

AllocatedImage Engine::createImage(VkExtent3D size, VkFormat format,
                                   VkImageUsageFlags usage, bool mipmap) {
  AllocatedImage image;
  image.format = format;
  image.extent = size;
  VkImageCreateInfo ci_image = vkinit::imageCreateInfo(format, usage, size);
  if (mipmap)
    ci_image.mipLevels = static_cast<uint32_t>(std::floor(
                             std::log2(std::max(size.width, size.height)))) +
                         1;

  VmaAllocationCreateInfo ci_alloc = {};
  ci_alloc.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  ci_alloc.requiredFlags =
      // Double check the allocation is in VRAM.
      VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK(vmaCreateImage(m_allocator, &ci_image, &ci_alloc, &image.image,
                          &image.allocation, nullptr));
  VkImageAspectFlags aspect_flags = format == VK_FORMAT_D32_SFLOAT
                                        ? VK_IMAGE_ASPECT_DEPTH_BIT
                                        : VK_IMAGE_ASPECT_COLOR_BIT;
  VkImageViewCreateInfo ci_view =
      vkinit::imageViewCreateInfo(format, image.image, aspect_flags);
  ci_view.subresourceRange.levelCount = ci_image.mipLevels;
  VK_CHECK(vkCreateImageView(m_device, &ci_view, nullptr, &image.view));
  return image;
}
AllocatedImage Engine::createImage(void *data, VkExtent3D size, VkFormat format,
                                   VkImageUsageFlags usage, bool mipmap) {
  // Assume R8G8B8A8 format.
  size_t data_size = size.depth * size.width * size.height * 4;
  AllocatedBuffer upload = createBuffer(
      data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  memcpy(upload.alloc_info.pMappedData, data, data_size);

  AllocatedImage new_image = createImage(
      size, format,
      usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      mipmap);

  immediateSubmit([&](VkCommandBuffer cmd) {
    vkutil::transitionImage(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy copy_region = {};
    copy_region.bufferOffset = 0;
    copy_region.bufferRowLength = 0;
    copy_region.bufferImageHeight = 0;
    copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_region.imageSubresource.mipLevel = 0;
    copy_region.imageSubresource.baseArrayLayer = 0;
    copy_region.imageSubresource.layerCount = 1;
    copy_region.imageExtent = size;
    vkCmdCopyBufferToImage(cmd, upload.buffer, new_image.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &copy_region);
    if (mipmap) {
      vkutil::generateMipmap(
          cmd, new_image.image,
          VkExtent2D{new_image.extent.width, new_image.extent.height});
    } else {
      vkutil::transitionImage(cmd, new_image.image,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
  });
  destroyBuffer(upload);
  return new_image;
}
void Engine::destroyImage(const AllocatedImage &image) {
  vkDestroyImageView(m_device, image.view, nullptr);
  vmaDestroyImage(m_allocator, image.image, image.allocation);
}
void Engine::updateScene() {}
#define VMA_IMPLEMENTATION
#include "engine.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "vk_initializers.h"
#include "vk_types.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include <VkBootstrap.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>

#include <chrono>
#include <thread>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

constexpr bool bUseValidationLayers = true;

bool isVisible(const RenderObject &obj, const glm::mat4 &view_proj) {
  std::array<glm::vec3, 8> corners{
      glm::vec3{1, 1, 1},   glm::vec3{1, 1, -1},   glm::vec3{1, -1, 1},
      glm::vec3{1, -1, -1}, glm::vec3{-1, 1, 1},   glm::vec3{-1, 1, -1},
      glm::vec3{-1, -1, 1}, glm::vec3{-1, -1, -1},
  };
  glm::vec3 min = {1.5, 1.5, 1.5};
  glm::vec3 max = {-1.5, -1.5, -1.5};
  glm::mat4 matrix = view_proj * obj.transform;

  for (int c = 0; c < 8; c++) {
    // The sphere is inscribed of this cube.
    glm::vec4 v =
        matrix *
        glm::vec4(obj.bound.origin + (corners[c] * obj.bound.radius), 1.f);
    // Perspective correction.
    v.x = v.x / v.w;
    v.y = v.y / v.w;
    v.z = v.z / v.w;
    min = glm::min(glm::vec3{v.x, v.y, v.z}, min);
    max = glm::max(glm::vec3{v.x, v.y, v.z}, max);
  }
  // check the clip space box is within the view
  if (min.z > 1.f || max.z < 0.f || min.x > 1.f || max.x < -1.f ||
      min.y > 1.f || max.y < -1.f) {
    return false;
  } else {
    return true;
  }
}

Engine &Engine::get() {
  static Engine engine; /// Singleton.
  return engine;
}
void Engine::init() {
  // We initialize SDL and create a window with it.
  SDL_Init(SDL_INIT_VIDEO);
  SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);
  // (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  window = SDL_CreateWindow("Vulkan Engine", window_extent.width,
                            window_extent.height, window_flags);

  initVulkan();
  initSwapchain();
  initCommands();
  initSyncStructures();
  initDescriptors();
  initPipelines();

  initImGui();

  initDefaultData();
  m_main_camera.init();
  is_initialized = true;
}

void Engine::cleanup() {
  if (is_initialized) {
    // Order matters, reversed of initialization.
    vkDeviceWaitIdle(m_device); // Wait for GPU to finish.
    m_loaded_scenes.clear();
    for (uint32_t i = 0; i < kFrameOverlap; i++) {
      // Cmd buffer is destroyed with pool it comes from.
      vkDestroyCommandPool(m_device, m_frames[i].cmd_pool, nullptr);
      vkDestroyFence(m_device, m_frames[i].render_fence, nullptr);
      vkDestroySemaphore(m_device, m_frames[i].render_semaphore, nullptr);
      vkDestroySemaphore(m_device, m_frames[i].swapchain_semaphore, nullptr);
      m_frames[i].deletion_queue.flush();
    }
    m_metal_rough_mat.clearResources(m_device);

    /**
     * @note  Global vma allocator must be deleted last.
     *        some allocations rely on this.
     */
    m_main_deletion_queue.flush();

    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyWindow(window);
  }
}

void Engine::draw() {
  stats.t_scene_update.begin();
  updateScene();
  stats.t_scene_update.end();
  stats.t_cpu_draw.begin();
  VK_CHECK(vkWaitForFences(m_device, 1, &getCurrentFrame().render_fence, true,
                           VK_ONE_SEC));
  // Free objects dedicated to this frame (in last iteration).
  getCurrentFrame().deletion_queue.flush();
  getCurrentFrame().frame_descriptors.clearPools(m_device);
  VK_CHECK(vkResetFences(m_device, 1, &getCurrentFrame().render_fence));

  // Request an image to draw to.
  uint32_t swapchain_img_idx;
  // Will signal the semaphore.
  VkResult e = vkAcquireNextImageKHR(m_device, m_swapchain, VK_ONE_SEC,
                                     getCurrentFrame().swapchain_semaphore,
                                     nullptr, &swapchain_img_idx);
  if (e == VK_ERROR_OUT_OF_DATE_KHR) {
    require_resize = true;
    stats.t_cpu_draw.end();
    return;
  }

  // Clear the cmd buffer.
  VkCommandBuffer cmd = getCurrentFrame().cmd_buffer_main;
  VK_CHECK(vkResetCommandBuffer(cmd, 0));
  // Record cmd. Bit is to tell vulkan buffer is used exactly once.
  VkCommandBufferBeginInfo cmd_begin_info =
      vkinit::cmdBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  m_draw_extent.width =
      std::min(m_color_image.extent.width, m_swapchain_extent.width) *
      m_render_scale;
  m_draw_extent.height =
      std::min(m_color_image.extent.height, m_swapchain_extent.height) *
      m_render_scale;

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
    drawGeometry(cmd);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        m_query_pool_timestamp, 3);
    // Copy to swapchain.
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        m_query_pool_timestamp, 4);
    vkutil::transitionImage(cmd, m_color_image.image,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transitionImage(cmd, m_swapchain_imgs[swapchain_img_idx],
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkutil::copyImage(cmd, m_color_image.image,
                      m_swapchain_imgs[swapchain_img_idx], m_draw_extent,
                      m_swapchain_extent);
    vkutil::transitionImage(cmd, m_swapchain_imgs[swapchain_img_idx],
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    // GUI drawing.
    drawImGui(cmd, m_swapchain_img_views[swapchain_img_idx]);
    vkutil::transitionImage(cmd, m_swapchain_imgs[swapchain_img_idx],
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        m_query_pool_timestamp, 5);
  }
  VK_CHECK(vkEndCommandBuffer(cmd));

  // Submit commands.
  VkCommandBufferSubmitInfo cmd_submit_info = vkinit::cmdBufferSubmitInfo(cmd);
  VkSemaphoreSubmitInfo wait_info = vkinit::semaphoreSubmitInfo(
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
      getCurrentFrame().swapchain_semaphore);
  VkSemaphoreSubmitInfo signal_info = vkinit::semaphoreSubmitInfo(
      VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, getCurrentFrame().render_semaphore);
  VkSubmitInfo2 submit_info =
      vkinit::submitInfo(&cmd_submit_info, &signal_info, &wait_info);
  VK_CHECK(vkQueueSubmit2(m_graphic_queue, 1, &submit_info,
                          getCurrentFrame().render_fence));

  // Present image.
  VkPresentInfoKHR present_info = vkinit::presentInfo();
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.pNext = nullptr;
  present_info.pSwapchains = &m_swapchain;
  present_info.swapchainCount = 1;
  present_info.pWaitSemaphores = &getCurrentFrame().render_semaphore;
  present_info.waitSemaphoreCount = 1;
  present_info.pImageIndices = &swapchain_img_idx;

  e = vkQueuePresentKHR(m_graphic_queue, &present_info);
  if (e == VK_ERROR_OUT_OF_DATE_KHR) {
    require_resize = true;
  }

  frame_number++;
  stats.t_cpu_draw.end();
  vkGetQueryPoolResults(m_device, m_query_pool_timestamp, 0,
                        static_cast<uint32_t>(m_timestamps.size()),
                        m_timestamps.size() * sizeof(uint64_t),
                        m_timestamps.data(), sizeof(uint64_t),
                        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
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
void Engine::drawGeometry(VkCommandBuffer cmd) {
  stats.n_triangles = 0;
  stats.n_drawcalls = 0;
  std::vector<size_t> opaque_index;
  opaque_index.reserve(m_main_draw_context.opaque_surfaces.size());
  // Visibility culling.
  for (size_t i = 0; i < m_main_draw_context.opaque_surfaces.size(); i++) {
    if (isVisible(m_main_draw_context.opaque_surfaces[i],
                  m_scene_data.view_proj))
      opaque_index.push_back(i);
  }
  // Should reduce rebinding?
  MaterialPipeline *last_pipeline = nullptr;
  MaterialInstance *last_material = nullptr;
  VkBuffer last_index_buffer = VK_NULL_HANDLE;
  auto drawObjet = [&](const RenderObject &r, VkDescriptorSet &frame_ds) {
    if (r.material != last_material) {
      last_material = r.material;
      if (r.material->p_pipeline != last_pipeline) {
        last_pipeline = r.material->p_pipeline;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          r.material->p_pipeline->pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                r.material->p_pipeline->layout, 0, 1, &frame_ds,
                                0, nullptr);
        VkViewport view_port = {
            .x = 0, .y = 0, .minDepth = 0.f, .maxDepth = 1.f};
        view_port.width = m_draw_extent.width;
        view_port.height = m_draw_extent.height;
        vkCmdSetViewport(cmd, 0, 1, &view_port);
        VkRect2D scissor = {};
        scissor.offset.x = scissor.offset.y = 0;
        scissor.extent.width = m_draw_extent.width;
        scissor.extent.height = m_draw_extent.height;
        vkCmdSetScissor(cmd, 0, 1, &scissor);
      }
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              r.material->p_pipeline->layout, 1, 1,
                              &r.material->ds, 0, nullptr);
    }
    if (r.index_buffer != last_index_buffer) {
      last_index_buffer = r.index_buffer;
      vkCmdBindIndexBuffer(cmd, r.index_buffer, 0, VK_INDEX_TYPE_UINT32);
    }
    GPUDrawPushConstants push_const;
    push_const.vertex_buffer_address = r.vertex_buffer_address;
    push_const.world_mat = r.transform;
    vkCmdPushConstants(cmd, r.material->p_pipeline->layout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(GPUDrawPushConstants), &push_const);
    vkCmdDrawIndexed(cmd, r.n_index, 1, r.first_index, 0, 0);
    stats.n_drawcalls += 1;
    stats.n_triangles += r.n_index / 3;
  };
  VkRenderingAttachmentInfo color_attach = vkinit::attachmentInfo(
      m_color_image.view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingAttachmentInfo depth_attach = vkinit::depthAttachmentInfo(
      m_depth_image.view, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  VkRenderingInfo i_render =
      vkinit::renderingInfo(m_draw_extent, &color_attach, &depth_attach);
  vkCmdBeginRendering(cmd, &i_render);
  {
    // vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
    //                   m_simple_mesh_pipeline);
    // Bind texture.
    VkDescriptorSet image_ds = getCurrentFrame().frame_descriptors.allocate(
        m_device, m_single_image_ds_layout);
    {
      DescriptorWriter writer;
      writer.writeImage(0, m_error_image.view, m_default_sampler_nearest,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.updateDescriptorSet(m_device, image_ds);
    }
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_simple_mesh_pipeline_layout, 0, 1, &image_ds, 0,
                            nullptr);

    AllocatedBuffer gpu_scene_data_buffer =
        createBuffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VMA_MEMORY_USAGE_CPU_TO_GPU);
    getCurrentFrame().deletion_queue.push(
        [=, this]() { destroyBuffer(gpu_scene_data_buffer); });
    GPUSceneData *scene_uniform_data =
        (GPUSceneData *)gpu_scene_data_buffer.allocation->GetMappedData();
    *scene_uniform_data = m_scene_data;
    VkDescriptorSet frame_ds = getCurrentFrame().frame_descriptors.allocate(
        m_device, m_GPU_scene_data_ds_layout);
    {

      DescriptorWriter writer;
      writer.writeBuffer(0, gpu_scene_data_buffer.buffer, sizeof(GPUSceneData),
                         0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
      writer.updateDescriptorSet(m_device, frame_ds);
    }

    // for (auto &r : m_main_draw_context.opaque_surfaces)
    //   drawObjet(r, frame_ds);
    for (auto &idx : opaque_index)
      drawObjet(m_main_draw_context.opaque_surfaces[idx], frame_ds);
    for (auto &r : m_main_draw_context.transparent_surfaces)
      drawObjet(r, frame_ds);
  }
  vkCmdEndRendering(cmd);
  m_main_draw_context.opaque_surfaces.clear();
  m_main_draw_context.transparent_surfaces.clear();
}
void Engine::run() {
  SDL_Event e;
  bool b_quit = false;
  while (!b_quit) {
    stats.t_frame.begin();

    // Event handling.
    while (SDL_PollEvent(&e) != 0) {
      // Close the window when alt-f4 or the X button.
      if (e.type == SDL_EVENT_QUIT)
        b_quit = true;
      if (e.type >= SDL_EVENT_WINDOW_FIRST && e.type <= SDL_EVENT_WINDOW_LAST) {
        if (e.window.type == SDL_EVENT_WINDOW_MINIMIZED)
          stop_rendering = true;
        if (e.window.type == SDL_EVENT_WINDOW_RESTORED)
          stop_rendering = false;
      }
      ImGui_ImplSDL3_ProcessEvent(&e);
      auto &io = ImGui::GetIO();
      if (io.WantCaptureMouse == false) // Imgui intercepting mouse.
        m_main_camera.processSDLEvent(e);
    }

    // Overall render configure.
    if (require_resize)
      resizeSwapchain();
    if (stop_rendering) {
      // Throttle the speed to avoid the endless spinning.
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    // UI update.
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    {
      if (ImGui::Begin("Panel")) {
        ImGui::SliderFloat("Render Scale", &m_render_scale, 0.3f, 1.f);
        auto &selected_pipeline = m_compute_pipelines[m_cur_comp_pipeline_idx];
        ImGui::Text("Selected Compute Pipeline: %s", selected_pipeline.name);
        ImGui::SliderInt("Effect Index", &m_cur_comp_pipeline_idx, 0,
                         m_compute_pipelines.size() - 1);
        ImGui::InputFloat4("data1", (float *)&selected_pipeline.data.data1);
        ImGui::InputFloat4("data2", (float *)&selected_pipeline.data.data2);
        ImGui::InputFloat4("data3", (float *)&selected_pipeline.data.data3);
        ImGui::InputFloat4("data4", (float *)&selected_pipeline.data.data4);

        float t_comp = static_cast<float>(m_timestamps[1] - m_timestamps[0]) *
                       m_timestamp_period / 1000000.f;
        float t_geom = static_cast<float>(m_timestamps[3] - m_timestamps[2]) *
                       m_timestamp_period / 1000000.f;
        float t_other = static_cast<float>(m_timestamps[5] - m_timestamps[4]) *
                        m_timestamp_period / 1000000.f;
        ImGui::Text("Stats:");
        ImGui::Text("\t#triangles      %d", stats.n_triangles);
        ImGui::Text("\t#drawcalls      %d", stats.n_drawcalls);
        ImGui::Text("CPU time:");
        ImGui::Text("\tframe time      %f ms", stats.t_frame.period_ms);
        ImGui::Text("\tscene update    %f ms", stats.t_scene_update.period_ms);
        ImGui::Text("\tCPU draw time   %f ms", stats.t_cpu_draw.period_ms);
        ImGui::Text("GPU time:");
        ImGui::Text("\tGPU compute     %f ms", t_comp);
        ImGui::Text("\tGPU geometry    %f ms", t_geom);
        ImGui::Text("\tGPU others      %f ms", t_other);

        auto &cam_pos = m_main_camera.position;
        ImGui::Text("Camera:");
        ImGui::Text("\tPosition (%.3f, %.3f, %.3f)", cam_pos.x, cam_pos.y,
                    cam_pos.z);
        ImGui::Text("\tPitch    %.3f", m_main_camera.pitch);
        ImGui::Text("\tYaw      %.3f", m_main_camera.yaw);
      }
      ImGui::End();
    }
    ImGui::Render();

    // Pipeline draw.
    draw();
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

  SDL_Vulkan_CreateSurface(window, m_instance, NULL, &m_surface);

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
          // For resetting query pool from host.
          .add_required_extension(VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME)
          .set_surface(m_surface)
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
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkb::destroy_debug_utils_messenger(m_instance, m_debug_msngr);
    vkDestroyInstance(m_instance, nullptr);
  });
}
void Engine::initSwapchain() {
  fmt::print("init swapchain\n");
  createSwapchain(window_extent.width, window_extent.height);

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

  m_main_deletion_queue.push([&]() {
    destroyImage(m_color_image);
    destroyImage(m_depth_image);
    destroySwapchain();
  });
}
void Engine::initCommands() {
  fmt::print("init commands\n");
  // Create a command pool for commands submitted to the graphics queue.
  // We also want the pool to allow for resetting of individual command buffers
  VkCommandPoolCreateInfo ci_cmd_pool = vkinit::cmdPoolCreateInfo(
      m_graphic_queue_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

  for (uint32_t i = 0; i < kFrameOverlap; i++) {
    VK_CHECK(vkCreateCommandPool(m_device, &ci_cmd_pool, nullptr,
                                 &m_frames[i].cmd_pool));
    VkCommandBufferAllocateInfo cmd_alloc_info =
        vkinit::cmdBufferAllocInfo(m_frames[i].cmd_pool, 1);
    VK_CHECK(vkAllocateCommandBuffers(m_device, &cmd_alloc_info,
                                      &m_frames[i].cmd_buffer_main));
  }

  // Init immediate cmd.
  VK_CHECK(
      vkCreateCommandPool(m_device, &ci_cmd_pool, nullptr, &m_imm_cmd_pool));
  VkCommandBufferAllocateInfo cmd_alloc_info =
      vkinit::cmdBufferAllocInfo(m_imm_cmd_pool, 1);
  VK_CHECK(vkAllocateCommandBuffers(m_device, &cmd_alloc_info, &m_imm_cmd));
  m_main_deletion_queue.push(
      [&]() { vkDestroyCommandPool(m_device, m_imm_cmd_pool, nullptr); });
}
void Engine::initSyncStructures() {
  // One fence to control when the gpu has finished rendering the frame.
  // 2 semaphores to synchronize rendering with swapchain.
  fmt::print("init sync structures\n");

  // The fence starts signalled so we can wait on it on the first frame.
  VkFenceCreateInfo ci_fence =
      vkinit::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
  VkSemaphoreCreateInfo ci_semaphore = vkinit::semaphoreCreateInfo();

  for (uint32_t i = 0; i < kFrameOverlap; i++) {
    VK_CHECK(
        vkCreateFence(m_device, &ci_fence, nullptr, &m_frames[i].render_fence));
    VK_CHECK(vkCreateSemaphore(m_device, &ci_semaphore, nullptr,
                               &m_frames[i].swapchain_semaphore));
    VK_CHECK(vkCreateSemaphore(m_device, &ci_semaphore, nullptr,
                               &m_frames[i].render_semaphore));
  }
  VK_CHECK(vkCreateFence(m_device, &ci_fence, nullptr, &m_imm_fence));
  m_main_deletion_queue.push(
      [&]() { vkDestroyFence(m_device, m_imm_fence, nullptr); });
}
void Engine::createSwapchain(int w, int h) {
  vkb::SwapchainBuilder swapchainBuilder{m_chosen_GPU, m_device, m_surface};
  m_swapchain_img_format = VK_FORMAT_B8G8R8A8_UNORM;
  vkb::Swapchain vkbSwapchain =
      swapchainBuilder
          .set_desired_format(VkSurfaceFormatKHR{
              .format = m_swapchain_img_format,
              .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
          // use v-sync present mode
          .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
          .set_desired_extent(w, h)
          .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
          .build()
          .value();

  m_swapchain_extent = vkbSwapchain.extent;
  m_swapchain = vkbSwapchain.swapchain;
  m_swapchain_imgs = vkbSwapchain.get_images().value();
  m_swapchain_img_views = vkbSwapchain.get_image_views().value();
}
void Engine::destroySwapchain() {
  // Images are deleted here.
  vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
  for (size_t i = 0; i < m_swapchain_img_views.size(); i++) {
    vkDestroyImageView(m_device, m_swapchain_img_views[i], nullptr);
  }
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

  for (size_t i = 0; i < kFrameOverlap; i++) {
    std::vector<DescriptorAllocator::PoolSizeRatio> frame_sizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
    };
    m_frames[i].frame_descriptors = {};
    m_frames[i].frame_descriptors.initPool(m_device, 1000, frame_sizes);
    m_main_deletion_queue.push(
        [&, i]() { m_frames[i].frame_descriptors.destroyPools(m_device); });
  }
}
void Engine::initPipelines() {
  // Compute pipelines.
  initBackgroundPipelines();
  // Graphics pipelines.
  initSimpleMeshPipeline();
  m_metal_rough_mat.buildPipelines(this);
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
void Engine::initSimpleMeshPipeline() {
  VkShaderModule mesh_shader_vert;
  VkShaderModule mesh_shader_frag;
  if (!vkutil::loadShaderModule("../../assets/shaders/simple_mesh.vert.spv",
                                m_device, &mesh_shader_vert)) {
    fmt::println("Error loading vert shader.");
  }
  if (!vkutil::loadShaderModule("../../assets/shaders/texture_image.frag.spv",
                                m_device, &mesh_shader_frag)) {
    fmt::println("Error loading frag shader.");
  }
  VkPushConstantRange buffer_range = {};
  buffer_range.offset = 0;
  buffer_range.size = sizeof(GPUDrawPushConstants);
  buffer_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  VkPipelineLayoutCreateInfo ci_pipeline_layout =
      vkinit::pipelineLayoutCreateInfo();
  ci_pipeline_layout.pPushConstantRanges = &buffer_range;
  ci_pipeline_layout.pushConstantRangeCount = 1;
  ci_pipeline_layout.pSetLayouts = &m_single_image_ds_layout;
  ci_pipeline_layout.setLayoutCount = 1;
  VK_CHECK(vkCreatePipelineLayout(m_device, &ci_pipeline_layout, nullptr,
                                  &m_simple_mesh_pipeline_layout));
  PipelineBuilder builder;
  builder.pipeline_layout = m_simple_mesh_pipeline_layout;
  builder.setShaders(mesh_shader_vert, mesh_shader_frag);
  builder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  builder.setPolygonMode(VK_POLYGON_MODE_FILL);
  builder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
  builder.setMultisamplingNone();
  // builder.disableBlending();
  builder.enableBlendingAlpha();
  builder.enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
  builder.setColorAttachFormat(m_color_image.format);
  builder.setDepthFormat(m_depth_image.format);
  m_simple_mesh_pipeline = builder.buildPipeline(m_device);

  vkDestroyShaderModule(m_device, mesh_shader_vert, nullptr);
  vkDestroyShaderModule(m_device, mesh_shader_frag, nullptr);
  m_main_deletion_queue.push([&]() {
    vkDestroyPipelineLayout(m_device, m_simple_mesh_pipeline_layout, nullptr);
    vkDestroyPipeline(m_device, m_simple_mesh_pipeline, nullptr);
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

void Engine::initImGui() {
  // 1: create descriptor pool for IMGUI.
  // Sizes are oversize, but copied from imgui demo itself.
  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, 100},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 100},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 100},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 100},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 100},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 100},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 100},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 100}};

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 100;
  pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
  pool_info.pPoolSizes = pool_sizes;

  VkDescriptorPool imgui_pool;
  VK_CHECK(vkCreateDescriptorPool(m_device, &pool_info, nullptr, &imgui_pool));

  // 2: initialize imgui library
  // Init the core structures of imgui
  ImGui::CreateContext();
  // Init imgui for SDL
  ImGui_ImplSDL3_InitForVulkan(window);
  // Init info of imgui for Vulkan
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = m_instance;
  init_info.PhysicalDevice = m_chosen_GPU;
  init_info.Device = m_device;
  init_info.Queue = m_graphic_queue;
  init_info.DescriptorPool = imgui_pool;
  init_info.MinImageCount = 3;
  init_info.ImageCount = 3;
  init_info.UseDynamicRendering = true;
  // Dynamic rendering parameters for imgui to use.
  init_info.PipelineRenderingCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
  init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  // GUI is drawn directly into swapchain.
  init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats =
      &m_swapchain_img_format;
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  ImGui_ImplVulkan_Init(&init_info);
  ImGui_ImplVulkan_CreateFontsTexture();
  // Pool value changes if capture using reference,
  // will cause validation layer to report.
  m_main_deletion_queue.push([this, imgui_pool]() {
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(m_device, imgui_pool, nullptr);
  });
}
void Engine::drawImGui(VkCommandBuffer cmd, VkImageView target_img_view) {
  VkRenderingAttachmentInfo color_attachment = vkinit::attachmentInfo(
      target_img_view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingInfo render_info =
      vkinit::renderingInfo(m_swapchain_extent, &color_attachment, nullptr);

  vkCmdBeginRendering(cmd, &render_info);
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
  vkCmdEndRendering(cmd);
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
GPUMeshBuffers Engine::uploadMesh(std::span<uint32_t> indices,
                                  std::span<Vertex> vertices) {
  const size_t kVertexBufferSize = vertices.size() * sizeof(Vertex);
  const size_t kIndexBufferSize = indices.size() * sizeof(uint32_t);

  GPUMeshBuffers mesh;
  mesh.vertex_buffer = createBuffer(
      kVertexBufferSize,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY);
  VkBufferDeviceAddressInfo i_device_address{
      .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      .buffer = mesh.vertex_buffer.buffer,
  };
  mesh.vertex_buffer_address =
      vkGetBufferDeviceAddress(m_device, &i_device_address);

  mesh.index_buffer = createBuffer(kIndexBufferSize,
                                   VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VMA_MEMORY_USAGE_GPU_ONLY);

  // Write data into a CPU-only staging buffer, then upload to GPU-only buffer.
  AllocatedBuffer staging =
      createBuffer(kVertexBufferSize + kIndexBufferSize,
                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
  void *data = staging.allocation->GetMappedData();
  memcpy(data, vertices.data(), kVertexBufferSize);
  memcpy((char *)data + kVertexBufferSize, indices.data(), kIndexBufferSize);
  immediateSubmit([&](VkCommandBuffer cmd) {
    VkBufferCopy vertex_copy = {};
    vertex_copy.dstOffset = 0;
    vertex_copy.srcOffset = 0;
    vertex_copy.size = kVertexBufferSize;
    vkCmdCopyBuffer(cmd, staging.buffer, mesh.vertex_buffer.buffer, 1,
                    &vertex_copy);
    VkBufferCopy index_copy = {};
    index_copy.dstOffset = 0;
    index_copy.srcOffset = kVertexBufferSize;
    index_copy.size = kIndexBufferSize;
    vkCmdCopyBuffer(cmd, staging.buffer, mesh.index_buffer.buffer, 1,
                    &index_copy);
  });
  destroyBuffer(staging);
  return mesh;
}

void Engine::initDefaultData() {
  uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
  m_white_image =
      createImage((void *)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                  VK_IMAGE_USAGE_SAMPLED_BIT);
  uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 1));
  m_black_image =
      createImage((void *)&black, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                  VK_IMAGE_USAGE_SAMPLED_BIT);
  uint32_t gray = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
  m_gray_image =
      createImage((void *)&gray, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
                  VK_IMAGE_USAGE_SAMPLED_BIT);
  uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
  std::array<uint32_t, 16 * 16> pixels; // 16x16 checkerboard texture.
  for (int x = 0; x < 16; x++)
    for (int y = 0; y < 16; y++)
      pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
  m_error_image =
      createImage(pixels.data(), VkExtent3D{16, 16, 1},
                  VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  VkSamplerCreateInfo ci_sampler = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
  };
  ci_sampler.magFilter = VK_FILTER_LINEAR;
  ci_sampler.minFilter = VK_FILTER_LINEAR;
  vkCreateSampler(m_device, &ci_sampler, nullptr, &m_default_sampler_linear);
  ci_sampler.magFilter = VK_FILTER_NEAREST;
  ci_sampler.minFilter = VK_FILTER_NEAREST;
  vkCreateSampler(m_device, &ci_sampler, nullptr, &m_default_sampler_nearest);

  m_main_deletion_queue.push([&]() {
    vkDestroySampler(m_device, m_default_sampler_linear, nullptr);
    vkDestroySampler(m_device, m_default_sampler_nearest, nullptr);

    destroyImage(m_white_image);
    destroyImage(m_black_image);
    destroyImage(m_gray_image);
    destroyImage(m_error_image);
  });

  std::string structurePath = {"../../assets/models/structure.glb"};
  auto structureFile = loadGltf(this, structurePath);
  assert(structureFile.has_value());
  m_loaded_scenes["structure"] = *structureFile;
}
void Engine::resizeSwapchain() {
  vkDeviceWaitIdle(m_device);
  destroySwapchain();
  int w, h;
  SDL_GetWindowSize(window, &w, &h);
  createSwapchain(w, h);
  require_resize = false;
}
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
void Engine::updateScene() {
  m_scene_data.prev_view_proj = m_scene_data.view_proj;
  m_main_camera.update();
  m_main_draw_context.opaque_surfaces.clear();
  m_main_draw_context.transparent_surfaces.clear();

  m_loaded_scenes["structure"]->draw(glm::mat4{1.f}, m_main_draw_context);

  m_scene_data.view = m_main_camera.getViewMatrix();
  m_scene_data.proj = glm::perspective(
      glm::radians(70.f), 1.f * window_extent.width / window_extent.height,
      0.1f, 10000.f);
  m_scene_data.proj[1][1] *= -1;
  m_scene_data.view_proj = m_scene_data.proj * m_scene_data.view;
  m_scene_data.ambient_color = glm::vec4(.1f);
  m_scene_data.sunlight_color = glm::vec4(1.f);
  m_scene_data.sunlight_dir = glm::vec4(0, 1, 0.5, 1.f);
}

void GLTFMetallicRoughness::buildPipelines(Engine *engine) {
  VkShaderModule mesh_frag_shader;
  if (!vkutil::loadShaderModule("../../assets/shaders/mesh.frag.spv",
                                engine->m_device, &mesh_frag_shader)) {
    fmt::println("Error when building the triangle fragment shader module");
  }
  VkShaderModule mesh_vert_shader;
  if (!vkutil::loadShaderModule("../../assets/shaders/mesh.vert.spv",
                                engine->m_device, &mesh_vert_shader)) {
    fmt::println("Error when building the triangle vertex shader module");
  }

  VkPushConstantRange range{};
  range.offset = 0;
  range.size = sizeof(GPUDrawPushConstants);
  range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  DescriptorLayoutBuilder ds_layout_builder;
  ds_layout_builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  ds_layout_builder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  ds_layout_builder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  ds_layout = ds_layout_builder.build(engine->m_device,
                                      VK_SHADER_STAGE_VERTEX_BIT |
                                          VK_SHADER_STAGE_FRAGMENT_BIT);
  VkDescriptorSetLayout layouts[] = {engine->m_GPU_scene_data_ds_layout,
                                     ds_layout};

  VkPipelineLayoutCreateInfo ci_pipeline_layout =
      vkinit::pipelineLayoutCreateInfo();
  ci_pipeline_layout.setLayoutCount = 2;
  ci_pipeline_layout.pSetLayouts = layouts;
  ci_pipeline_layout.pPushConstantRanges = &range;
  ci_pipeline_layout.pushConstantRangeCount = 1;
  VkPipelineLayout layout;
  VK_CHECK(vkCreatePipelineLayout(engine->m_device, &ci_pipeline_layout,
                                  nullptr, &layout));
  // Same layout but different configure.
  pipeline_opaque.layout = layout;
  pipeline_transparent.layout = layout;

  PipelineBuilder pipeline_builder;
  pipeline_builder.setShaders(mesh_vert_shader, mesh_frag_shader);
  pipeline_builder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  pipeline_builder.setPolygonMode(VK_POLYGON_MODE_FILL);
  pipeline_builder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
  pipeline_builder.setMultisamplingNone();
  pipeline_builder.disableBlending();
  pipeline_builder.enableDepthTest(true, VK_COMPARE_OP_LESS_OR_EQUAL);
  pipeline_builder.setColorAttachFormat(engine->m_color_image.format);
  pipeline_builder.setDepthFormat(engine->m_depth_image.format);
  pipeline_builder.pipeline_layout = layout;
  pipeline_opaque.pipeline = pipeline_builder.buildPipeline(engine->m_device);
  pipeline_builder.enableBlendingAdd();
  pipeline_builder.enableDepthTest(false, VK_COMPARE_OP_LESS_OR_EQUAL);
  pipeline_transparent.pipeline =
      pipeline_builder.buildPipeline(engine->m_device);

  vkDestroyShaderModule(engine->m_device, mesh_frag_shader, nullptr);
  vkDestroyShaderModule(engine->m_device, mesh_vert_shader, nullptr);
}
void GLTFMetallicRoughness::clearResources(VkDevice device) {
  vkDestroyDescriptorSetLayout(device, ds_layout, nullptr);
  // Same layout.
  vkDestroyPipelineLayout(device, pipeline_transparent.layout, nullptr);
  vkDestroyPipeline(device, pipeline_opaque.pipeline, nullptr);
  vkDestroyPipeline(device, pipeline_transparent.pipeline, nullptr);
}

MaterialInstance
GLTFMetallicRoughness::writeMaterial(VkDevice device, MaterialPass pass,
                                     const MaterialResources &resources,
                                     DescriptorAllocator &d_allocator) {
  MaterialInstance mat_data;
  mat_data.pass_type = pass;
  switch (pass) {
  case MaterialPass::BasicMainColor:
    mat_data.p_pipeline = &pipeline_opaque;
    break;
  case MaterialPass::BasicTransparent:
    mat_data.p_pipeline = &pipeline_transparent;
    break;
  case MaterialPass::Others:
    fmt::println("Other material.");
    mat_data.p_pipeline = &pipeline_opaque;
    break;
  }

  mat_data.ds = d_allocator.allocate(device, ds_layout);

  writer.clear();
  writer.writeBuffer(0, resources.data_buffer, sizeof(MaterialConstants),
                     resources.data_buffer_offset,
                     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  writer.writeImage(1, resources.color_image.view, resources.color_sampler,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.writeImage(2, resources.metal_rough_image.view,
                    resources.metal_rough_sampler,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.updateDescriptorSet(device, mat_data.ds);

  return mat_data;
}

#define VMA_IMPLEMENTATION
#include "GPU/GPU.hpp"

#include "utils/vk/allocation.hpp"
#include "utils/vk/images.hpp"
#include "utils/vk/buffers.hpp"
#include "utils/vk/FrameData.hpp"
#include "utils/vk/pipelines.hpp"
#include "utils/log.hpp"
#include "Scene/Scene.hpp"

#include <stb_image.h>
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>

namespace vrtr {
void GPU::init(SDL_Window *window) {
  LOGI("GPU init.");
  m_window = window;
  int w, h;
  SDL_GetWindowSize(m_window, &w, &h);
  m_window_extent.width = w;
  m_window_extent.height = h;

  initVulkan();
  initSwapchain();
  initOffScreenImages();
  initCommands();
  initSyncStructures();
  initDescriptors();
  initRenderPass();
  initPipelines();
  initFrameBuffers();
  initTextures();
}

void GPU::initVulkan() {
  LOGI("Init Vulkan.");

  LOGI("Build Vulkan instance.");
  vkb::InstanceBuilder builder;
  auto instance_result = builder.set_app_name("vrtr")
                             .request_validation_layers(kUseValidation)
                             .use_default_debug_messenger()
                             .require_api_version(1, 3, 0)
                             .build();
  auto vkb_instance = instance_result.value();
  m_instance = vkb_instance.instance;
  m_debug_messenger = vkb_instance.debug_messenger;

  SDL_Vulkan_CreateSurface(m_window, m_instance, NULL, &m_surface);

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
  vkb::PhysicalDeviceSelector selector{vkb_instance};
  vkb::PhysicalDevice physical_device =
      selector.set_minimum_version(1, 3)
          .set_required_features_13(features13)
          .set_required_features_12(features12)
          // For resetting query pool from host.
          .add_required_extension(VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME)
          .set_surface(m_surface)
          .select()
          .value();

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
  vmaCreateAllocator(&ci_alloc, &m_mem_allocator);

  m_deletion_queue.push([&]() {
    vmaDestroyAllocator(m_mem_allocator);
    vkDestroyDevice(m_device, nullptr);
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    vkb::destroy_debug_utils_messenger(m_instance, m_debug_messenger);
    vkDestroyInstance(m_instance, nullptr);
  });
}

void GPU::initSwapchain() {
  createSwapchain(m_window_extent.width, m_window_extent.height);
  m_deletion_queue.push([&]() { destroySwapchain(); });
}

void GPU::createSwapchain(int w, int h) {
  vkb::SwapchainBuilder swapchainBuilder{m_chosen_GPU, m_device, m_surface};
  m_swapchain_format = VK_FORMAT_B8G8R8A8_UNORM;
  vkb::Swapchain vkbSwapchain =
      swapchainBuilder
          .set_desired_format(VkSurfaceFormatKHR{
              .format = m_swapchain_format,
              .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
          // use v-sync present mode
          .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
          .set_desired_extent(w, h)
          .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
          .build()
          .value();

  m_swapchain_extent = vkbSwapchain.extent;
  m_swapchain = vkbSwapchain.swapchain;
  m_swapchain_images = vkbSwapchain.get_images().value();
  m_swapchain_image_views = vkbSwapchain.get_image_views().value();
}
void GPU::destroySwapchain() {
  vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
  for (size_t i = 0; i < m_swapchain_image_views.size(); i++) {
    vkDestroyImageView(m_device, m_swapchain_image_views[i], nullptr);
  }
}

void GPU::initOffScreenImages() {
  {
    vkimage::ImageBuilder builder;
    m_color_image =
        builder
            .setExtent(m_swapchain_extent.width, m_swapchain_extent.height, 1)
            .setFormat(VK_FORMAT_R16G16B16A16_SFLOAT)
            .addUsage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            .addUsage(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .addUsage(VK_IMAGE_USAGE_STORAGE_BIT) // For compute shaders.
            .addUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            .build(m_device, m_mem_allocator);
  }
  {
    vkimage::ImageBuilder builder;
    m_depth_image =
        builder
            .setExtent(m_swapchain_extent.width, m_swapchain_extent.height, 1)
            .setFormat(VK_FORMAT_D32_SFLOAT)
            .addUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
            .build(m_device, m_mem_allocator);
  }
  m_deletion_queue.push([&]() {
    m_color_image.destroy();
    m_depth_image.destroy();
  });
}

void GPU::initCommands() {
  LOGI("Init commands.");
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
  m_deletion_queue.push(
      [&]() { vkDestroyCommandPool(m_device, m_imm_cmd_pool, nullptr); });
}

void GPU::initSyncStructures() {
  // One fence to control when the gpu has finished rendering the frame.
  // 2 semaphores to synchronize rendering with swapchain.
  LOGI("Init sync structures.");

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
  m_deletion_queue.push(
      [&]() { vkDestroyFence(m_device, m_imm_fence, nullptr); });
}

void GPU::initDescriptors() {
  std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}};
  // Descriptor pool with 10 des sets, 1 image each.
  m_descriptor_allocator.initPool(m_device, 10, sizes);
  {
    DescriptorLayoutBuilder builder;
    builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    builder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    m_desc_set_layouts.scene_data = builder.build(
        m_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  }

  m_deletion_queue.push([&]() {
    m_descriptor_allocator.destroyPools(m_device);
    vkDestroyDescriptorSetLayout(m_device, m_desc_set_layouts.scene_data,
                                 nullptr);
  });

  /// Frame-dedicated pools are accessed on-the-fly.
  for (size_t i = 0; i < kFrameOverlap; i++) {
    std::vector<DescriptorAllocator::PoolSizeRatio> frame_sizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
    };
    m_frames[i].descriptor_allocator = {};
    m_frames[i].descriptor_allocator.initPool(m_device, 1000, frame_sizes);
    m_deletion_queue.push(
        [&, i]() { m_frames[i].descriptor_allocator.destroyPools(m_device); });
  }
}

void GPU::initRenderPass() {
  VkAttachmentDescription color_attach{};
  color_attach.format = VK_FORMAT_R16G16B16A16_SFLOAT;
  color_attach.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attach.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attach.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attach.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attach.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference color_attach_ref{};
  color_attach_ref.attachment = 0;
  color_attach_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription depth_attach{};
  depth_attach.format = VK_FORMAT_D32_SFLOAT;
  depth_attach.samples = VK_SAMPLE_COUNT_1_BIT;
  depth_attach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attach.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attach.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depth_attach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attach.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_attach.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_attach_ref{};
  depth_attach_ref.attachment = 1;
  depth_attach_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_attach_ref;
  subpass.pDepthStencilAttachment = &depth_attach_ref;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                             VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  std::array<VkAttachmentDescription, 2> attachments = {color_attach,
                                                        depth_attach};
  VkRenderPassCreateInfo ci_render_pass{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  ci_render_pass.attachmentCount = static_cast<uint32_t>(attachments.size());
  ci_render_pass.pAttachments = attachments.data();
  ci_render_pass.subpassCount = 1;
  ci_render_pass.pSubpasses = &subpass;
  ci_render_pass.dependencyCount = 1;
  ci_render_pass.pDependencies = &dependency;
  VK_CHECK(
      vkCreateRenderPass(m_device, &ci_render_pass, nullptr, &m_render_pass));
  m_deletion_queue.push(
      [&]() { vkDestroyRenderPass(m_device, m_render_pass, nullptr); });
}

void GPU::initPipelines() { initGraphicPipeline(); }

void GPU::initGraphicPipeline() {
  /// A single-subpass render pass pipeline.
  VkPipelineLayoutCreateInfo pipelineLayoutInfo =
      vkinit::pipelineLayoutCreateInfo();
  pipelineLayoutInfo.setLayoutCount = 1;
  pipelineLayoutInfo.pSetLayouts = &m_desc_set_layouts.scene_data;
  pipelineLayoutInfo.pushConstantRangeCount = 0;
  pipelineLayoutInfo.pPushConstantRanges = nullptr;
  VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr,
                                  &m_pipeline_layout));
  m_deletion_queue.push(
      [&]() { vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr); });
  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                               VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  auto v_binding_description = Vertex::getVertexBindingDescription();
  auto v_attribute_descriptions = Vertex::getVertexAttributeDescriptions();
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.pVertexBindingDescriptions = &v_binding_description;
  vertexInputInfo.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(v_attribute_descriptions.size());
  vertexInputInfo.pVertexAttributeDescriptions =
      v_attribute_descriptions.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)m_swapchain_extent.width;
  viewport.height = (float)m_swapchain_extent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = m_swapchain_extent;

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;
  rasterizer.depthBiasConstantFactor = 0.0f; // Optional
  rasterizer.depthBiasClamp = 0.0f;          // Optional
  rasterizer.depthBiasSlopeFactor = 0.0f;    // Optional

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  multisampling.minSampleShading = 1.0f;          // Optional
  multisampling.pSampleMask = nullptr;            // Optional
  multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
  multisampling.alphaToOneEnable = VK_FALSE;      // Optional

  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;  // Optional
  colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;             // Optional
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;  // Optional
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
  colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;             // Optional
  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 0.0f; // Optional
  colorBlending.blendConstants[1] = 0.0f; // Optional
  colorBlending.blendConstants[2] = 0.0f; // Optional
  colorBlending.blendConstants[3] = 0.0f; // Optional

  VkShaderModule triangle_vert{};
  if (!vkutil::loadShaderModule("../../assets/shaders/spv/default.vert.spv",
                                m_device, &triangle_vert))
    fmt::println("failed loading shader");
  VkShaderModule triangle_frag{};
  if (!vkutil::loadShaderModule("../../assets/shaders/spv/default.frag.spv",
                                m_device, &triangle_frag))
    fmt::println("failed loading shader");
  VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
  vertShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = triangle_vert;
  vertShaderStageInfo.pName = "main";
  VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
  fragShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = triangle_frag;
  fragShaderStageInfo.pName = "main";
  VkPipelineShaderStageCreateInfo ci_shader_stages[] = {vertShaderStageInfo,
                                                        fragShaderStageInfo};

  VkPipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_TRUE;
  depthStencil.depthWriteEnable = VK_TRUE;
  /// [0, 1] for [near, far].
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.minDepthBounds = 0.0f; // Optional
  depthStencil.maxDepthBounds = 1.0f; // Optional
  depthStencil.stencilTestEnable = VK_FALSE;
  depthStencil.front = {}; // Optional
  depthStencil.back = {};  // Optional

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = ci_shader_stages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = m_pipeline_layout;
  pipelineInfo.renderPass = m_render_pass;
  pipelineInfo.subpass = 0;
  VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                     nullptr, &m_pipeline));

  vkDestroyShaderModule(m_device, triangle_vert, nullptr);
  vkDestroyShaderModule(m_device, triangle_frag, nullptr);
  m_deletion_queue.push(
      [&]() { vkDestroyPipeline(m_device, m_pipeline, nullptr); });
}

void GPU::initFrameBuffers() {
  std::array<VkImageView, 2> attach_views = {m_color_image.view,
                                             m_depth_image.view};

  VkFramebufferCreateInfo ci_framebuffer{};
  ci_framebuffer.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  ci_framebuffer.renderPass = m_render_pass;
  ci_framebuffer.attachmentCount = static_cast<uint32_t>(attach_views.size());
  ci_framebuffer.pAttachments = attach_views.data();
  ci_framebuffer.width = m_swapchain_extent.width;
  ci_framebuffer.height = m_swapchain_extent.height;
  ci_framebuffer.layers = 1;

  VK_CHECK(
      vkCreateFramebuffer(m_device, &ci_framebuffer, nullptr, &m_framebuffer));
  m_deletion_queue.push(
      [&]() { vkDestroyFramebuffer(m_device, m_framebuffer, nullptr); });

  vkbuffer::BufferBuilder builder;
  builder = builder.addBufferUsage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
                .setMemoryUsage(VMA_MEMORY_USAGE_CPU_TO_GPU)
                .setSize(sizeof(SceneData));
  for (int i = 0; i < kFrameOverlap; i++) {
    m_frames[i].scene_data_buffer = builder.build(m_mem_allocator);
  }
}

void GPU::draw() {
  VK_CHECK(vkWaitForFences(m_device, 1, &getCurrentFrame().render_fence, true,
                           VK_ONE_SEC));
  // Free objects dedicated to this frame (in last iteration).
  getCurrentFrame().deletion_queue.flush();
  getCurrentFrame().descriptor_allocator.clearPools(m_device);
  VK_CHECK(vkResetFences(m_device, 1, &getCurrentFrame().render_fence));

  // Request an image to draw to.
  uint32_t swapchain_img_idx;
  // Will signal the semaphore.
  VkResult e = vkAcquireNextImageKHR(m_device, m_swapchain, VK_ONE_SEC,
                                     getCurrentFrame().swapchain_semaphore,
                                     nullptr, &swapchain_img_idx);
  if (e == VK_ERROR_OUT_OF_DATE_KHR) {
    LOGE("No impl for swapchain resizing.");
  }

  // Clear the cmd buffer.
  VkCommandBuffer cmd = getCurrentFrame().cmd_buffer_main;
  VK_CHECK(vkResetCommandBuffer(cmd, 0));
  // Record cmd. Bit is to tell vulkan buffer is used exactly once.
  VkCommandBufferBeginInfo cmd_begin_info =
      vkinit::cmdBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  auto draw_extent = m_swapchain_extent;

  VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_begin_info));
  { // Drawing commands.
    vkimage::transitionImage(cmd, m_color_image.image,
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    recordCmdBuffer(cmd);
    vkimage::transitionImage(cmd, m_color_image.image,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkimage::transitionImage(cmd, m_swapchain_images[swapchain_img_idx],
                             VK_IMAGE_LAYOUT_UNDEFINED,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkimage::copyImage(cmd, m_color_image.image,
                       m_swapchain_images[swapchain_img_idx], draw_extent,
                       m_swapchain_extent);
    vkimage::transitionImage(cmd, m_swapchain_images[swapchain_img_idx],
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkimage::transitionImage(cmd, m_swapchain_images[swapchain_img_idx],
                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                             VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
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
    LOGE("No impl for swapchain resizing.");
  }

  m_frame_number++;
}

void GPU::recordCmdBuffer(VkCommandBuffer cmd) {

  VkRenderPassBeginInfo bi_render_pass{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  bi_render_pass.renderPass = m_render_pass;
  bi_render_pass.framebuffer = m_framebuffer;
  bi_render_pass.renderArea.offset = {0, 0};
  bi_render_pass.renderArea.extent = m_swapchain_extent;

  std::array<VkClearValue, 2> clear_values{};
  clear_values[0].color = {{0.f, 0.f, 0.f, 1.f}};
  clear_values[1].depthStencil = {1.0f, 0};
  bi_render_pass.clearValueCount = static_cast<uint32_t>(clear_values.size());
  bi_render_pass.pClearValues = clear_values.data();

  vkCmdBeginRenderPass(cmd, &bi_render_pass, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
  {
    SceneData *scene_uniform_data =
        (SceneData *)getCurrentFrame()
            .scene_data_buffer.allocation->GetMappedData();
    *scene_uniform_data = m_scene_data;
    VkDescriptorSet frame_ds = getCurrentFrame().descriptor_allocator.allocate(
        m_device, m_desc_set_layouts.scene_data);
    DescriptorWriter writer;
    writer.writeBuffer(0, getCurrentFrame().scene_data_buffer.buffer,
                       sizeof(SceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.writeImage(1, m_texture.view, m_default_sampler_linear,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.updateDescriptorSet(m_device, frame_ds);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipeline_layout, 0, 1, &frame_ds, 0, nullptr);
  }
  VkBuffer vertex_buffers[] = {m_vertex_buffer.buffer};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, offsets);
  vkCmdBindIndexBuffer(cmd, m_index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(m_swapchain_extent.width);
  viewport.height = static_cast<float>(m_swapchain_extent.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = m_swapchain_extent;
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  vkCmdDrawIndexed(cmd, m_num_indices, 1, 0, 0, 0);

  vkCmdEndRenderPass(cmd);
}

void GPU::uploadScene(const Scene &scene) {
  LOGI("Uploading scene to GPU.");
  {
    LOGI("Uploading vertex data.");
    auto vertices = scene.getVertices();
    size_t vertices_size = sizeof(vertices[0]) * vertices.size();
    {
      vkbuffer::BufferBuilder builder;
      m_vertex_buffer =
          builder.setSize(vertices_size)
              .addBufferUsage(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
              .addBufferUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
              /// TODO Use Buffer Device Address to upload vertices.
              // .addBufferUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
              .setMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
              .build(m_mem_allocator);
      // VkBufferDeviceAddressInfo i_device_address{
      //     .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
      //     .buffer = m_vertex_buffer.buffer,
      // };
      // m_vertex_buffer_address =
      //     vkGetBufferDeviceAddress(m_device, &i_device_address);
    }
    {
      AllocatedBuffer staging;
      vkbuffer::BufferBuilder builder;
      staging = builder.setSize(vertices_size)
                    .addBufferUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
                    .setMemoryUsage(VMA_MEMORY_USAGE_CPU_ONLY)
                    .build(m_mem_allocator);
      void *data = staging.allocation->GetMappedData();
      memcpy(data, vertices.data(), vertices_size);
      immediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertex_copy = {};
        vertex_copy.dstOffset = 0;
        vertex_copy.srcOffset = 0;
        vertex_copy.size = vertices_size;
        vkCmdCopyBuffer(cmd, staging.buffer, m_vertex_buffer.buffer, 1,
                        &vertex_copy);
      });
      staging.destroy();
    }
  }
  {
    LOGI("Uploading index data.");
    auto indices = scene.getIndices();
    size_t indices_size = sizeof(indices[0]) * indices.size();
    m_num_indices = indices.size();
    {
      vkbuffer::BufferBuilder builder;
      m_index_buffer = builder.setSize(indices_size)
                           .addBufferUsage(VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
                           .addBufferUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT)
                           .setMemoryUsage(VMA_MEMORY_USAGE_GPU_ONLY)
                           .build(m_mem_allocator);
    }
    {
      AllocatedBuffer staging;
      vkbuffer::BufferBuilder builder;
      staging = builder.setSize(indices_size)
                    .addBufferUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
                    .setMemoryUsage(VMA_MEMORY_USAGE_CPU_ONLY)
                    .build(m_mem_allocator);
      void *data = staging.allocation->GetMappedData();
      memcpy(data, indices.data(), indices_size);
      immediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy index_copy = {};
        index_copy.dstOffset = 0;
        index_copy.srcOffset = 0;
        index_copy.size = indices_size;
        vkCmdCopyBuffer(cmd, staging.buffer, m_index_buffer.buffer, 1,
                        &index_copy);
      });
      staging.destroy();
    }
  }

  m_deletion_queue.push([&]() {
    m_vertex_buffer.destroy();
    m_index_buffer.destroy();
  });
}

void GPU::updateScene(const Scene &scene) {
  m_scene_data = scene.getSceneData();
}

AllocatedImage GPU::uploadImage(void *data, VkExtent3D size, VkFormat format,
                                VkImageUsageFlags usage, bool mipmap) {
  // Assume R8G8B8A8 format.
  size_t data_size = size.depth * size.width * size.height * 4;
  vkbuffer::BufferBuilder buffer_builder;
  AllocatedBuffer upload = buffer_builder.setSize(data_size)
                               .addBufferUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
                               .setMemoryUsage(VMA_MEMORY_USAGE_CPU_TO_GPU)
                               .build(m_mem_allocator);
  memcpy(upload.alloc_info.pMappedData, data, data_size);

  vkimage::ImageBuilder image_builder;
  AllocatedImage new_image =
      image_builder.setExtent(size.width, size.height, size.depth)
          .setUsage(usage)
          .addUsage(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
          .addUsage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
          .setFormat(format)
          .build(m_device, m_mem_allocator, mipmap);

  immediateSubmit([&](VkCommandBuffer cmd) {
    vkimage::transitionImage(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
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
      vkimage::generateMipmap(
          cmd, new_image.image,
          VkExtent2D{new_image.extent.width, new_image.extent.height});
    } else {
      vkimage::transitionImage(cmd, new_image.image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
  });
  upload.destroy();
  return new_image;
}

void GPU::initTextures() {
  int w, h, n_channels;
  /// TODO Check if mipmap is real.
  bool mipmap = true;
  unsigned char *data = stbi_load("../../assets/images/default_texture.png", &w,
                                  &h, &n_channels, 4);
  if (data) {
    VkExtent3D img_size;
    img_size.width = w;
    img_size.height = h;
    img_size.depth = 1;
    m_texture = uploadImage(data, img_size, VK_FORMAT_R8G8B8A8_UNORM,
                            VK_IMAGE_USAGE_SAMPLED_BIT, mipmap);
    stbi_image_free(data);
  } else {
    LOGE("Error loading texture file.");
  }

  VkSamplerCreateInfo ci_sampler = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
  };
  ci_sampler.magFilter = VK_FILTER_LINEAR;
  ci_sampler.minFilter = VK_FILTER_LINEAR;
  vkCreateSampler(m_device, &ci_sampler, nullptr, &m_default_sampler_linear);
  ci_sampler.magFilter = VK_FILTER_NEAREST;
  ci_sampler.minFilter = VK_FILTER_NEAREST;
  vkCreateSampler(m_device, &ci_sampler, nullptr, &m_default_sampler_nearest);

  m_deletion_queue.push([&]() {
    m_texture.destroy();
    vkDestroySampler(m_device, m_default_sampler_linear, nullptr);
    vkDestroySampler(m_device, m_default_sampler_nearest, nullptr);
  });
}

void GPU::immediateSubmit(std::function<void(VkCommandBuffer cmd)> &&func) {
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

void GPU::deinit() {
  LOGI("GPU deinit.");
  vkDeviceWaitIdle(m_device);
  for (uint32_t i = 0; i < kFrameOverlap; i++) {
    // Cmd buffer is destroyed with pool it comes from.
    vkDestroyCommandPool(m_device, m_frames[i].cmd_pool, nullptr);
    vkDestroyFence(m_device, m_frames[i].render_fence, nullptr);
    vkDestroySemaphore(m_device, m_frames[i].render_semaphore, nullptr);
    vkDestroySemaphore(m_device, m_frames[i].swapchain_semaphore, nullptr);
    m_frames[i].scene_data_buffer.destroy();
    m_frames[i].deletion_queue.flush();
  }

  m_deletion_queue.flush();
}
} // namespace vrtr
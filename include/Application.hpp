#pragma once

#include "GPU/GPU.hpp"
#include "Scene/Scene.hpp"
#include "utils/json.hpp"
#include "Window.hpp"
#include "rendering/RenderContext.hpp"
#include "utils/vk/pipelines.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <set>
#include <optional>

constexpr std::array<const char *, 1> REQUIRED_LAYERS{
    "VK_LAYER_KHRONOS_validation"};

constexpr std::array<const char *, 1> DEVICE_EXTENSIONS{
    vk::KHRSwapchainExtensionName};

#ifdef NDEBUG
constexpr bool ENABLE_VALIDATION_LAYER = false;
#else
constexpr bool ENABLE_VALIDATION_LAYER = true;
#endif

namespace vrtr {
struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;

  bool AllFound() {
    return graphicsFamily.has_value() && presentFamily.has_value();
  }
};
class Application {
public:
  /**
   * @brief Create application.
   * create Vulkan
   * create render context
   * load the Scene
   * create RenderPipeline with ShaderModule(s)
   * create Camera
   * create Gui
   */
  void Create(const Json &config, const Window *window) {
    mWindow = window;

    // mGpu.init(mWindow->getSDLHandle());
    // mGpu.uploadScene(mScene);

    CreateVulkan();
    CreateRenderContext();
    LoadScene();
    CreateRenderPass();
    CreateGraphicsPipeline();
    CreateCamera();
    CreateGui();
  }

  void Destroy() {
    // ...
    // mGpu.deinit();
  }

  void TickLogic(float delta) {
    // Some CPU side update.
    // mScene.tick(delta);
  }

  void TickRender(float delta) {
    /**
     * render context
     *    begin frame, wait and acquire image in
     * request cmd buffer
     * update stats and gui
     * do render
     * present
     */
    // mGpu.updateScene(mScene);
    // mGpu.draw();
  }

private:
  void CreateVulkan();
  void CreateRenderContext();
  void LoadScene();
  void CreateRenderPass(); // TODO Remove this.
  void CreateGraphicsPipeline();
  void CreateCamera();
  void CreateGui();

  // TODO Use unique_ptr?
  const Window *mWindow;
  Scene mScene;

  GPU mGpu;

  vk::raii::Context mVkContext;
  // TODO See core/hpp_instance.h for more encapsule.
  vk::raii::Instance mVkInstance{nullptr};
  vk::raii::DebugUtilsMessengerEXT mDebugMessenger{nullptr};
  vk::raii::SurfaceKHR mSurface{nullptr};
  vk::raii::PhysicalDevice mPhysicalDevice{nullptr};
  vk::raii::Device mDevice{nullptr};
  vk::raii::Queue mGraphicsQueue{nullptr};
  vk::raii::Queue mPresentQueue{nullptr};

  std::vector<vk::SurfaceFormatKHR> surfacePriorityList = {
      {vk::Format::eR8G8B8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear},
      {vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear}};
  std::vector<vk::PresentModeKHR> presentModePriorityList{
      vk::PresentModeKHR::eFifo, vk::PresentModeKHR::eMailbox,
      vk::PresentModeKHR::eImmediate};
  vk::raii::SwapchainKHR mSwapchain{nullptr};
  std::vector<vk::Image> mSwapchainImages;
  std::vector<vk::raii::ImageView> mSwapchainImageViews;
  vk::Format mSwapchainImageFormat{};
  vk::Extent2D mSwapchainExtent{};
  std::unique_ptr<RenderContext> mRenderContext;

  vk::raii::RenderPass mRenderPass{nullptr};
  std::vector<vk::raii::Framebuffer> mSwapchainFramebuffers;
  vk::raii::PipelineLayout mPipelineLayout{nullptr};
  vk::raii::Pipeline mGraphicsPipeline{nullptr};

  DeletionQueue mDeletionQueue{};
};
} // namespace vrtr

static std::vector<const char *>
GetRequiredExtensions(const vrtr::Window *window) {
  std::vector<const char *> extVec = window->GetRequiredExtensions();
  // For mac.
  // extVec.emplace_back(vk::KHRPortabilityEnumerationExtensionName);
  if constexpr (ENABLE_VALIDATION_LAYER) {
    extVec.emplace_back(vk::EXTDebugUtilsExtensionName);
  }

  return extVec;
}

static VKAPI_ATTR uint32_t VKAPI_CALL DebugMessageCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    vk::DebugUtilsMessageTypeFlagsEXT messageTypes,
    vk::DebugUtilsMessengerCallbackDataEXT const *pCallbackData,
    void *pUserData) {
  switch (messageSeverity) {
  case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
    LOGD("validation layer: {}", pCallbackData->pMessage);
    break;
  case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
    LOGI("validation layer: {}", pCallbackData->pMessage);
    break;
  case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
    LOGW("validation layer: {}", pCallbackData->pMessage);
    break;
  case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
    LOGE("validation layer: {}", pCallbackData->pMessage);
    break;
  }
  return false;
}

static vk::DebugUtilsMessengerCreateInfoEXT GetDebugMessengerCreateInfo() {
  vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
  vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(
      vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
      vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
      vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
  vk::DebugUtilsMessengerCreateInfoEXT createInfo(
      {},                   // flag
      severityFlags,        // messageSeverity
      messageTypeFlags,     // messageType
      &DebugMessageCallback // user defined callback
  );
  return createInfo;
}

static vrtr::QueueFamilyIndices
FindQueueFamilies(const vk::raii::PhysicalDevice &phyDevice,
                  const vk::raii::SurfaceKHR &surface) {
  vrtr::QueueFamilyIndices indices{};
  auto queueFamilies = phyDevice.getQueueFamilyProperties();
  for (uint32_t i = 0; const auto &family : queueFamilies) {
    if (family.queueFlags & vk::QueueFlagBits::eGraphics)
      indices.graphicsFamily = i;

    if (phyDevice.getSurfaceSupportKHR(i, surface))
      indices.presentFamily = i;

    if (indices.AllFound())
      break;
    i++;
  }
  return indices;
}

static bool
CheckDeviceExtensionSupport(const vk::raii::PhysicalDevice &physicalDevice) {
  // std::vector<vk::ExtensionProperties>
  const auto availableExtensions =
      physicalDevice.enumerateDeviceExtensionProperties();
  std::set<std::string> requiredExtensions(DEVICE_EXTENSIONS.begin(),
                                           DEVICE_EXTENSIONS.end());
  for (const auto &extension : availableExtensions) {
    requiredExtensions.erase(extension.extensionName);
  }
  return requiredExtensions.empty();
}

struct SwapchainSupportDetails {
  vk::SurfaceCapabilitiesKHR capabilities;
  std::vector<vk::SurfaceFormatKHR> formats;
  std::vector<vk::PresentModeKHR> presentModes;
};
static SwapchainSupportDetails
QuerySwapchainSupport(const vk::raii::PhysicalDevice &physicalDevice,
                      const vk::raii::SurfaceKHR &surface) {
  SwapchainSupportDetails details;
  details.capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
  details.formats = physicalDevice.getSurfaceFormatsKHR(surface);
  details.presentModes = physicalDevice.getSurfacePresentModesKHR(surface);

  return details;
}
static vk::SurfaceFormatKHR ChooseSwapSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR> &availableFormats,
    const std::vector<vk::SurfaceFormatKHR> &formatPriorityList) {
  for (const auto &targetFmt : formatPriorityList) {
    for (const auto &givenFmt : availableFormats) {
      if (givenFmt.format == targetFmt.format &&
          givenFmt.colorSpace == targetFmt.colorSpace)
        return givenFmt;
    }
  }
  return availableFormats[0];
}
static vk::PresentModeKHR
ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availableModes,
                      const std::vector<vk::PresentModeKHR> &modePriorityList) {
  for (const auto &targetMode : modePriorityList) {
    for (const auto &givenMode : modePriorityList) {
      if (givenMode == targetMode)
        return targetMode;
    }
  }
  return vk::PresentModeKHR::eFifo;
}

static vk::Extent2D
ChooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities,
                 const vrtr::Window &window) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  }
  auto actualExtent = window.GetWindowSize();
  actualExtent.width =
      std::clamp(actualExtent.width, capabilities.minImageExtent.width,
                 capabilities.maxImageExtent.width);
  actualExtent.height =
      std::clamp(actualExtent.height, capabilities.minImageExtent.height,
                 capabilities.maxImageExtent.height);
  return actualExtent;
}

static bool IsDeviceSuitable(const vk::raii::PhysicalDevice &physicalDevice,
                             const vk::raii::SurfaceKHR &surface) {
  // Device name, vulkan version, etc.
  // vk::PhysicalDeviceProperties properties = phyDevice.getProperties();
  // Supported features.
  // vk::PhysicalDeviceFeatures features = phyDevice.getFeatures();

  auto queueIndices = FindQueueFamilies(physicalDevice, surface);
  auto extensionOk = CheckDeviceExtensionSupport(physicalDevice);
  auto swapchainDetails = QuerySwapchainSupport(physicalDevice, surface);
  auto swapchainOk =
      swapchainDetails.formats.size() && swapchainDetails.presentModes.size();

  return queueIndices.AllFound() && extensionOk && swapchainOk;
}

inline void vrtr::Application::CreateVulkan() {
  /**
   * instance, surface
   * physical device, logical device
   * creating the Swapchain
   */
  LOGI("create vk");

  {
    LOGI("create vk instance");
    vk::ApplicationInfo appInfo;
    appInfo.pApplicationName = "vrtr";
    appInfo.applicationVersion = 1;
    appInfo.pEngineName = "vrtrEngine";
    appInfo.engineVersion = 1;
    appInfo.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char *> extVec = GetRequiredExtensions(mWindow);

    vk::InstanceCreateInfo instanceCi;
    instanceCi.setPApplicationInfo(&appInfo)
        .setFlags({})
        .setPEnabledExtensionNames(extVec);

    // Outside the constexpr if to ensure pointer life span.
    auto debugMessengerCi = GetDebugMessengerCreateInfo();
    if constexpr (ENABLE_VALIDATION_LAYER) {
      LOGI("use validation layer");
      const auto layers = mVkContext.enumerateInstanceLayerProperties();
      std::set<std::string> requiredLayers(REQUIRED_LAYERS.begin(),
                                           REQUIRED_LAYERS.end());
      for (const auto &layer : layers)
        requiredLayers.erase(layer.layerName);

      if (requiredLayers.empty()) {

      } else {
        throw std::runtime_error{"required validation layer not supported"};
      }
      // Device-wise layer is deprecated.
      instanceCi.setPEnabledLayerNames(REQUIRED_LAYERS);
      instanceCi.setPNext(&debugMessengerCi);
    }
    mVkInstance = mVkContext.createInstance(instanceCi);
  }
  {
    LOGI("setup debug messenger");
    if (ENABLE_VALIDATION_LAYER) {
      mDebugMessenger = mVkInstance.createDebugUtilsMessengerEXT(
          GetDebugMessengerCreateInfo());
    }
  }
  {
    LOGI("create surface");
    // *mVkInstance to get a vk::Instance, then implicitly convert to
    // VKInstance.
    auto cSurface = mWindow->CreateCurface(*mVkInstance);
    mSurface = vk::raii::SurfaceKHR{mVkInstance, cSurface};
  }
  {
    LOGI("select physical device");
    auto physicalDevices = mVkInstance.enumeratePhysicalDevices();
    if (physicalDevices.empty())
      throw std::runtime_error{"no physical devices for vk"};
    for (const auto &d : physicalDevices) {
      if (IsDeviceSuitable(d, mSurface)) {
        mPhysicalDevice = d;
        break;
      }
    }
  }
  {
    LOGI("create queue and logical device");
    std::vector<vk::DeviceQueueCreateInfo> queueCis{};

    auto indices = FindQueueFamilies(mPhysicalDevice, mSurface);
    std::set<uint32_t> uniqueFamilyIndices = {indices.graphicsFamily.value(),
                                              indices.presentFamily.value()};

    constexpr float priority = 1.0;
    for (auto idx : uniqueFamilyIndices) {
      vk::DeviceQueueCreateInfo ci{};
      queueCis.emplace_back(
          ci.setQueueFamilyIndex(idx).setQueuePriorities(priority));
    }

    vk::PhysicalDeviceFeatures phyDeviceFeatures{};
    vk::DeviceCreateInfo deviceCi{};
    deviceCi.setQueueCreateInfos(queueCis);
    deviceCi.setPEnabledFeatures(&phyDeviceFeatures);
    deviceCi.setPEnabledExtensionNames(DEVICE_EXTENSIONS);

    mDevice = mPhysicalDevice.createDevice(deviceCi);
    mGraphicsQueue = mDevice.getQueue(indices.graphicsFamily.value(), 0);
    mPresentQueue = mDevice.getQueue(indices.presentFamily.value(), 0);
  }

  mDeletionQueue.push([&]() {});
}
inline void vrtr::Application::CreateRenderContext() {
  // TODO Encapsule this.
  {
    LOGI("create swapchain");
    const auto [capabilities, formats, presentModes] =
        QuerySwapchainSupport(mPhysicalDevice, mSurface);
    const auto surfaceFormat =
        ChooseSwapSurfaceFormat(formats, surfacePriorityList);
    const auto presentMode =
        ChooseSwapPresentMode(presentModes, presentModePriorityList);
    const auto extent = ChooseSwapExtent(capabilities, *mWindow);

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 &&
        imageCount > capabilities.maxImageCount)
      imageCount = capabilities.maxImageCount;

    vk::SwapchainCreateInfoKHR swapCi{};
    swapCi.setSurface(mSurface)
        .setMinImageCount(imageCount)
        .setImageFormat(surfaceFormat.format)
        .setImageColorSpace(surfaceFormat.colorSpace)
        .setPresentMode(presentMode)
        .setImageExtent(extent)
        .setImageArrayLayers(1)
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
        .setPreTransform(capabilities.currentTransform)
        .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
        .setClipped(vk::True)
        .setOldSwapchain(nullptr);

    // In case graphics and present queue are different.
    const auto [graphicsFamily, presentFamily] =
        FindQueueFamilies(mPhysicalDevice, mSurface);
    std::vector<uint32_t> queueFamilyIndices{graphicsFamily.value(),
                                             presentFamily.value()};
    if (graphicsFamily != presentFamily) {
      swapCi.setImageSharingMode(vk::SharingMode::eConcurrent)
          .setQueueFamilyIndices(queueFamilyIndices);
    } else {
      swapCi.setImageSharingMode(vk::SharingMode::eExclusive);
    }

    mSwapchain = mDevice.createSwapchainKHR(swapCi);
    mSwapchainImages = mSwapchain.getImages();

    mSwapchainImageFormat = surfaceFormat.format;
    mSwapchainExtent = extent;
  }
  {
    LOGI("create swapchain imageviews");
    vk::ImageSubresourceRange subResRange{};
    subResRange.setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseMipLevel(0)
        .setLevelCount(1)
        .setBaseArrayLayer(0)
        .setLayerCount(1);
    vk::ImageViewCreateInfo imageViewCi{};
    imageViewCi.setViewType(vk::ImageViewType::e2D)
        .setFormat(mSwapchainImageFormat)
        .setSubresourceRange(subResRange);
    mSwapchainImageViews.reserve(mSwapchainImages.size());
    for (const auto &image : mSwapchainImages) {
      imageViewCi.setImage(image);
      mSwapchainImageViews.emplace_back(mDevice.createImageView(imageViewCi));
    }
  }
}
inline void vrtr::Application::LoadScene() {
  // TODO ECS scene.
}
inline void vrtr::Application::CreateRenderPass() {
  {
    LOGI("create render pass");
    vk::AttachmentDescription colorAttachment{};
    colorAttachment.setFormat(mSwapchainImageFormat)
        .setSamples(vk::SampleCountFlagBits::e1);
    colorAttachment.setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    colorAttachment.setInitialLayout(vk::ImageLayout::eUndefined)
        .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

    // For subpass.
    vk::AttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0; // Index of attachment desc in array.
    colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::SubpassDescription subpass{};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.setColorAttachments({
        colorAttachmentRef,
    });

    // Subpass dependency is omitted by now since there's only one subpass.

    vk::RenderPassCreateInfo renderPassCi{};
    renderPassCi.setAttachments({
        colorAttachment,
    });
    renderPassCi.setSubpasses({
        subpass,
    });
    mRenderPass = mDevice.createRenderPass(renderPassCi);
  }
  {
    LOGI("create frame buffers for swapchain");
    mSwapchainFramebuffers.reserve(mSwapchainImageViews.size());
    vk::FramebufferCreateInfo framebufferCi;
    framebufferCi.renderPass = mRenderPass;
    framebufferCi.width = mSwapchainExtent.width;
    framebufferCi.height = mSwapchainExtent.height;
    framebufferCi.layers = 1;
    for (const auto &imageView : mSwapchainImageViews) {
      framebufferCi.setAttachments({
          *imageView,
      });
      mSwapchainFramebuffers.emplace_back(
          mDevice.createFramebuffer(framebufferCi));
    }
  }
}
inline void vrtr::Application::CreateGraphicsPipeline() {
  {
    LOGI("load shaders");
    vk::raii::ShaderModule vertShaderModule = vkutil::loadShaderModule(
        "../../assets/shaders/spv/triangle.vert.spv", mDevice);
    vk::raii::ShaderModule fragShaderModule = vkutil::loadShaderModule(
        "../../assets/shaders/spv/triangle.frag.spv", mDevice);

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
    vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";
    vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
    fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";
    const auto shaderStages = {vertShaderStageInfo, fragShaderStageInfo};

    LOGI("create graphics pipeline");
    const auto dynamicStates = {vk::DynamicState::eViewport,
                                vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState;
    dynamicState.setDynamicStates(dynamicStates);

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
    inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
    inputAssembly.primitiveRestartEnable = false;

    vk::Viewport viewport{
        0.0f,                                        // x
        0.0f,                                        // y
        static_cast<float>(mSwapchainExtent.width),  // width
        static_cast<float>(mSwapchainExtent.height), // height
        0.0f,                                        // minDepth
        1.0f                                         // maxDepth
    };
    vk::Rect2D scissor{{0, 0}, // offset
                       mSwapchainExtent};
    vk::PipelineViewportStateCreateInfo viewportState;
    viewportState
        .setViewports({
            viewport,
        })
        .setScissors({
            scissor,
        });
    vk::PipelineRasterizationStateCreateInfo rasterizer;
    rasterizer.depthClampEnable =
        false; // Clamp or discard for out-of-plane frags.
    rasterizer.rasterizerDiscardEnable = false; // Or geometry never passes.
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f; // Need wideLines feature if gt 1.0f.
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace =
        vk::FrontFace::eClockwise;      // How to judge the facing.
    rasterizer.depthBiasEnable = false; // Shadow maps may want enabled.

    // Disable MSAA.
    vk::PipelineMultisampleStateCreateInfo multisampling;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.sampleShadingEnable = false;

    /// Blending is disabled, see
    /// https://mysvac.github.io/vulkan-hpp-tutorial/md/01/22_fixfunction/#_9
    /// for complex blending.
    vk::PipelineColorBlendAttachmentState colorBlendAttachment;
    colorBlendAttachment.blendEnable = false; // default
    colorBlendAttachment.colorWriteMask =
        (vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
         vk::ColorComponentFlagBits::eB |
         vk::ColorComponentFlagBits::eA); // or AllFlags
    vk::PipelineColorBlendStateCreateInfo colorBlending;
    colorBlending.logicOpEnable = false;
    colorBlending.logicOp = vk::LogicOp::eCopy;
    colorBlending.setAttachments(colorBlendAttachment);

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
    mPipelineLayout = mDevice.createPipelineLayout(pipelineLayoutInfo);

    vk::GraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.setStages({
        shaderStages,
    });
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr; // Optional
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = mPipelineLayout;
    pipelineInfo.renderPass = mRenderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = nullptr; // Optional
    pipelineInfo.basePipelineIndex = -1;       // Optional

    mGraphicsPipeline = mDevice.createGraphicsPipeline(nullptr, pipelineInfo);
    mDeletionQueue.push([&]() {

    });
  }
}
inline void vrtr::Application::CreateCamera() {}
inline void vrtr::Application::CreateGui() {}
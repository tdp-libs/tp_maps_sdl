#include "tp_maps_sdl/Vulkan.h"

#include "tp_utils/DebugUtils.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>
#include <SDL2/SDL_vulkan.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan/vk_enum_string_helper.h>

#include <set>

namespace tp_maps_sdl
{

//##################################################################################################
struct Vulkan::Private
{
  SDL_Window* window;
  std::string title;

  bool ok{true};

  const std::vector<const char*> validationLayers =
  {
    ///has bug
    //"VK_LAYER_LUNARG_standard_validation"
  };

  VkInstance instance;
  VkSurfaceKHR surface;
  VkPhysicalDevice physicalDevice;

  uint32_t graphicsQueueFamilyIndex{0};
  uint32_t presentQueueFamilyIndex{0};

  VkDevice device;
  VkQueue graphicsQueue;
  VkQueue presentQueue;

  std::vector<VkImage> swapchainImages;
  uint32_t swapchainImageCount;
  VkSurfaceFormatKHR surfaceFormat;
  VkExtent2D swapchainSize;
  VkSwapchainKHR swapchain;
  VkSurfaceCapabilitiesKHR surfaceCapabilities;

  std::vector<VkImageView> swapchainImageViews;

  VkFormat depthFormat;
  VkImage depthImage;
  VkDeviceMemory depthImageMemory;
  VkImageView depthImageView;

  VkRenderPass renderPass;

  std::vector<VkFramebuffer> swapchainFramebuffers;

  VkCommandPool commandPool;
  std::vector<VkCommandBuffer> commandBuffers;

  VkSemaphore imageAvailableSemaphore;
  VkSemaphore renderingFinishedSemaphore;

  std::vector<VkFence> fences;

  PFN_vkCreateDebugReportCallbackEXT SDL2_vkCreateDebugReportCallbackEXT = nullptr;
  VkDebugReportCallbackEXT debugCallback;


  //################################################################################################
  Private(SDL_Window* window_, const std::string& title_):
    window(window_),
    title(title_)
  {
    //-- Create Instance ---------------------------------------------------------------------------
    {
      unsigned int extensionCount = 0;
      SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);
      std::vector<const char *> extensionNames(extensionCount);
      SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensionNames.data());

      tpDebug() << "Extension names:";
      for(auto extensionName : extensionNames)
        tpDebug() << " - " << extensionName;

      VkApplicationInfo appInfo = {};
      appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
      appInfo.pApplicationName = title.c_str();
      appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
      appInfo.pEngineName = "tp_maps_sdl";
      appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
      appInfo.apiVersion = VK_API_VERSION_1_0;

      VkInstanceCreateInfo instanceCreateInfo = {};
      instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
      instanceCreateInfo.pApplicationInfo = &appInfo;
      instanceCreateInfo.enabledLayerCount = validationLayers.size();
      instanceCreateInfo.ppEnabledLayerNames = validationLayers.data();
      instanceCreateInfo.enabledExtensionCount = extensionNames.size();
      instanceCreateInfo.ppEnabledExtensionNames = extensionNames.data();

      tpDebug() << "Create instance:";
      VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
      tpDebug() << " - " << string_VkResult(result);

      if(result != VK_SUCCESS)
      {
        tpWarning() << "Failed to create Vulkan instance.";
        ok = false;
      }
    }

    //-- Create Debug ------------------------------------------------------------------------------
    {
      SDL2_vkCreateDebugReportCallbackEXT = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(SDL_Vulkan_GetVkGetInstanceProcAddr());

      VkDebugReportCallbackCreateInfoEXT debugCallbackCreateInfo = {};
      debugCallbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
      debugCallbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
      debugCallbackCreateInfo.pfnCallback = vulkanReportFunc;

      SDL2_vkCreateDebugReportCallbackEXT(instance, &debugCallbackCreateInfo, 0, &debugCallback);
    }

    //-- Create Surface ----------------------------------------------------------------------------
    {
      SDL_Vulkan_CreateSurface(window, instance, &surface);
    }

    //-- Select Physical Device --------------------------------------------------------------------
    {
      std::vector<VkPhysicalDevice> physicalDevices;
      uint32_t physicalDeviceCount = 0;

      vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
      physicalDevices.resize(physicalDeviceCount);
      vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());

      tpDebug() << "List physical devices:";
      for(const auto& physicalDevice : physicalDevices)
      {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        tpDebug() << " - " << properties.deviceName;
      }

      physicalDevice = physicalDevices[2];
    }

    //-- Select Queue Family -----------------------------------------------------------------------
    {
      std::vector<VkQueueFamilyProperties> queueFamilyProperties;
      uint32_t queueFamilyCount;

      vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
      queueFamilyProperties.resize(queueFamilyCount);
      vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

      int graphicIndex = -1;
      int presentIndex = -1;

      int i = 0;
      tpDebug() << "Queue familes:";
      for(const auto& queueFamily : queueFamilyProperties)
      {
        tpDebug() << " - queueFlags: " << string_VkQueueFlags(queueFamily.queueFlags) <<
                     " queueCount: " << queueFamily.queueCount <<
                     " timestampValidBits: " << queueFamily.timestampValidBits <<
                     " minImageTransferGranularity: ("
                     "width: " << queueFamily.minImageTransferGranularity.width <<
                     " height: " << queueFamily.minImageTransferGranularity.height <<
                     " depth: " << queueFamily.minImageTransferGranularity.depth << ")";

        if(queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
          graphicIndex = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
        if(queueFamily.queueCount > 0 && presentSupport)
        {
          presentIndex = i;
        }

        if(graphicIndex != -1 && presentIndex != -1)
        {
          break;
        }

        i++;
      }
    }

    //-- Create Device -----------------------------------------------------------------------------
    {
      const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
      const float queue_priority[] = { 1.0f };

      std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
      std::set<uint32_t> uniqueQueueFamilies = { graphicsQueueFamilyIndex, presentQueueFamilyIndex };

      float queuePriority = queue_priority[0];
      for(int queueFamily : uniqueQueueFamilies)
      {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
      }

      VkDeviceQueueCreateInfo queueCreateInfo = {};
      queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
      queueCreateInfo.queueCount = 1;
      queueCreateInfo.pQueuePriorities = &queuePriority;

      //https://en.wikipedia.org/wiki/Anisotropic_filtering
      VkPhysicalDeviceFeatures deviceFeatures = {};
      deviceFeatures.samplerAnisotropy = VK_TRUE;

      VkDeviceCreateInfo createInfo = {};
      createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
      createInfo.pQueueCreateInfos = &queueCreateInfo;
      createInfo.queueCreateInfoCount = queueCreateInfos.size();
      createInfo.pQueueCreateInfos = queueCreateInfos.data();
      createInfo.pEnabledFeatures = &deviceFeatures;
      createInfo.enabledExtensionCount = deviceExtensions.size();
      createInfo.ppEnabledExtensionNames = deviceExtensions.data();

      createInfo.enabledLayerCount = validationLayers.size();
      createInfo.ppEnabledLayerNames = validationLayers.data();

      vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);

      vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphicsQueue);
      vkGetDeviceQueue(device, presentQueueFamilyIndex, 0, &presentQueue);
    }

    //-- Create Swap Chain -------------------------------------------------------------------------
    {
      vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);

      std::vector<VkSurfaceFormatKHR> surfaceFormats;
      uint32_t surfaceFormatsCount;
      vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface,
                                           &surfaceFormatsCount,
                                           nullptr);
      surfaceFormats.resize(surfaceFormatsCount);
      vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface,
                                           &surfaceFormatsCount,
                                           surfaceFormats.data());

      tpDebug() << "Surface formats:";
      for(const auto& surfaceFormat : surfaceFormats)
      {
        tpDebug() << " - format: " << string_VkFormat(surfaceFormat.format) <<
                     " colorSpace: " << string_VkColorSpaceKHR(surfaceFormat.colorSpace);

        if(surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM)
          this->surfaceFormat = surfaceFormat;
      }

      if(surfaceFormat.format != VK_FORMAT_B8G8R8A8_UNORM)
      {
        tpWarning() << "Error surfaceFormat.format != VK_FORMAT_B8G8R8A8_UNORM.";
        ok = false;
        return;
      }

      int width = 0;
      int height = 0;
      SDL_Vulkan_GetDrawableSize(window, &width, &height);

      width = std::clamp(width, int(surfaceCapabilities.minImageExtent.width), int(surfaceCapabilities.maxImageExtent.width));
      height = std::clamp(height, int(surfaceCapabilities.minImageExtent.height), int(surfaceCapabilities.maxImageExtent.height));

      swapchainSize.width = width;
      swapchainSize.height = height;

      uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
      if(surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount)
        imageCount = surfaceCapabilities.maxImageCount;

      VkSwapchainCreateInfoKHR createInfo = {};
      createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
      createInfo.surface = surface;
      createInfo.minImageCount = surfaceCapabilities.minImageCount;
      createInfo.imageFormat = surfaceFormat.format;
      createInfo.imageColorSpace = surfaceFormat.colorSpace;
      createInfo.imageExtent = swapchainSize;
      createInfo.imageArrayLayers = 1;
      createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

      uint32_t queueFamilyIndices[] = {graphicsQueueFamilyIndex, presentQueueFamilyIndex};
      if (graphicsQueueFamilyIndex != presentQueueFamilyIndex)
      {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
      }
      else
      {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      }

      createInfo.preTransform = surfaceCapabilities.currentTransform;
      createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
      createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
      createInfo.clipped = VK_TRUE;

      vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain);

      vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);
      swapchainImages.resize(swapchainImageCount);
      vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());
    }

    //-- Create Image Views ------------------------------------------------------------------------
    {
      swapchainImageViews.resize(swapchainImages.size());

      for(uint32_t i = 0; i < swapchainImages.size(); i++)
      {
        swapchainImageViews[i] = createImageView(swapchainImages[i], surfaceFormat.format, VK_IMAGE_ASPECT_COLOR_BIT);
      }
    }

    //-- Setup Depth Stencil -----------------------------------------------------------------------
    {
      VkBool32 validDepthFormat = getSupportedDepthFormat(physicalDevice, &depthFormat);
      createImage(swapchainSize.width,
                  swapchainSize.height,
                  VK_FORMAT_D32_SFLOAT_S8_UINT,
                  VK_IMAGE_TILING_OPTIMAL,
                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                  depthImage,
                  depthImageMemory);
      depthImageView = createImageView(depthImage, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    //-- Create Render Pass ------------------------------------------------------------------------
    {
      std::vector<VkAttachmentDescription> attachments(2);

      attachments[0].format = surfaceFormat.format;
      attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
      attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

      attachments[1].format = depthFormat;
      attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
      attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

      VkAttachmentReference colorReference = {};
      colorReference.attachment = 0;
      colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      VkAttachmentReference depthReference = {};
      depthReference.attachment = 1;
      depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

      VkSubpassDescription subpassDescription = {};
      subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
      subpassDescription.colorAttachmentCount = 1;
      subpassDescription.pColorAttachments = &colorReference;
      subpassDescription.pDepthStencilAttachment = &depthReference;
      subpassDescription.inputAttachmentCount = 0;
      subpassDescription.pInputAttachments = nullptr;
      subpassDescription.preserveAttachmentCount = 0;
      subpassDescription.pPreserveAttachments = nullptr;
      subpassDescription.pResolveAttachments = nullptr;

      std::vector<VkSubpassDependency> dependencies(1);

      dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
      dependencies[0].dstSubpass = 0;
      dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
      dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
      dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

      VkRenderPassCreateInfo renderPassInfo = {};
      renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
      renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
      renderPassInfo.pAttachments = attachments.data();
      renderPassInfo.subpassCount = 1;
      renderPassInfo.pSubpasses = &subpassDescription;
      renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
      renderPassInfo.pDependencies = dependencies.data();

      vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
    }

    //-- Create Framebuffers -----------------------------------------------------------------------
    {
      swapchainFramebuffers.resize(swapchainImageViews.size());

      for (size_t i = 0; i < swapchainImageViews.size(); i++)
      {
        std::vector<VkImageView> attachments(2);
        attachments[0] = swapchainImageViews[i];
        attachments[1] = depthImageView;

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapchainSize.width;
        framebufferInfo.height = swapchainSize.height;
        framebufferInfo.layers = 1;

        if(auto r=vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFramebuffers[i]); r != VK_SUCCESS)
        {
          tpWarning() << "Failed to create framebuffer!";
          tpWarning() << " - Result: " << string_VkResult(r);
          ok = false;
          return;
        }
      }
    }

    //-- Create Command Pool -----------------------------------------------------------------------
    {
      VkCommandPoolCreateInfo createInfo = {};
      createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
      createInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
      vkCreateCommandPool(device, &createInfo, nullptr, &commandPool);
    }

    //-- Create Command Buffers --------------------------------------------------------------------
    {
      VkCommandBufferAllocateInfo allocateInfo = {};
      allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      allocateInfo.commandPool = commandPool;
      allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      allocateInfo.commandBufferCount = swapchainImageCount;

      commandBuffers.resize(swapchainImageCount);
      vkAllocateCommandBuffers(device, &allocateInfo, commandBuffers.data());
    }

    //-- Create Semaphores -------------------------------------------------------------------------
    {
      createSemaphore(&imageAvailableSemaphore);
      createSemaphore(&renderingFinishedSemaphore);
    }

    //-- Create Fences -----------------------------------------------------------------------------
    {
      uint32_t i;
      fences.resize(swapchainImageCount);
      for(i = 0; i < swapchainImageCount; i++)
      {
        VkFenceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateFence(device, &createInfo, nullptr, &fences[i]);
      }
    }
  }

  //################################################################################################
  static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanReportFunc(VkDebugReportFlagsEXT flags,
                                                         VkDebugReportObjectTypeEXT objType,
                                                         uint64_t obj,
                                                         size_t location,
                                                         int32_t code,
                                                         const char* layerPrefix,
                                                         const char* msg,
                                                         void* userData)
  {
    tpWarning() <<  "VULKAN VALIDATION: " << msg;
    return VK_FALSE;
  }

  //################################################################################################
  VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
  {
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create texture image view!");
    }

    return imageView;
  }

  //################################################################################################
  VkBool32 getSupportedDepthFormat(VkPhysicalDevice physicalDevice, VkFormat *depthFormat)
  {
    std::vector<VkFormat> depthFormats = {
      VK_FORMAT_D32_SFLOAT_S8_UINT,
      VK_FORMAT_D32_SFLOAT,
      VK_FORMAT_D24_UNORM_S8_UINT,
      VK_FORMAT_D16_UNORM_S8_UINT,
      VK_FORMAT_D16_UNORM
    };

    for(auto& format : depthFormats)
    {
      VkFormatProperties formatProps;
      vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProps);
      if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
      {
        *depthFormat = format;
        return true;
      }
    }

    return false;
  }

  //################################################################################################
  void createImage(uint32_t width,
                   uint32_t height,
                   VkFormat format,
                   VkImageTiling tiling,
                   VkImageUsageFlags usage,
                   VkMemoryPropertyFlags properties,
                   VkImage& image,
                   VkDeviceMemory& imageMemory)
  {
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS)
    {
      throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
      throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(device, image, imageMemory, 0);
  }

  //################################################################################################
  uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
  {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
      if((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
      {
        return i;
      }
    }

    throw std::runtime_error("failed to find suitable memory type!");
  }

  //################################################################################################
  void createSemaphore(VkSemaphore *semaphore)
  {
    VkSemaphoreCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore(device, &createInfo, nullptr, semaphore);
  }
};

//##################################################################################################
Vulkan::Vulkan(SDL_Window* window, const std::string& title):
  d(new Private(window, title))
{

}

//##################################################################################################
Vulkan::~Vulkan()
{
  delete d;
}

}

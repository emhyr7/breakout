#include "breakout.h"

#include "breakout_allocators.cpp"

class Breakout
{
public: 
  void execute(void)
  {
    this->initialize();

    /* the main loop */
    Scratch scratch;
    context.allocator->derive(&scratch);
    for (;;)
    {
      /* retrieve and process window messages */
      {
        int message_result = GetMessage(&this->win32_window_message, 0, 0, 0);
        if (!message_result) this->requested_quit = 1;
        else if (message_result < 0)
        {
          /* resolve error here */
          throw std::runtime_error("failed to process message.");
        }
        TranslateMessage(&this->win32_window_message);
        DispatchMessage(&this->win32_window_message);
      }

      scratch.die();
      if (this->requested_quit) break;
    }

    this->terminate();
  }

private:
  static constexpr char     application_name[]  =  "BREAKOUT";
  static constexpr uint32_t application_version = VK_MAKE_VERSION(1, 0, 0);

  bool requested_quit : 1 = false;

  /* Win32 properties */
  HINSTANCE    win32_instance;
  STARTUPINFOW win32_startup_info;
  HWND         win32_window;
  MSG          win32_window_message;

  /* Vulkan properties */
  VkInstance               vk_instance           = 0;
  VkDebugUtilsMessengerEXT vk_debug_messenger    = 0;
  VkSurfaceKHR             vk_surface            = 0;
  VkPhysicalDevice         vk_physical_device    = 0;
  VkDevice                 vk_device             = 0;
  VkQueue                  vk_graphics_queue     = 0;
  VkQueue                  vk_presentation_queue = 0;
  VkSwapchainKHR           vk_swapchain          = 0;

/******************************************************************************/
  
  static LRESULT CALLBACK win32_process_window_message(HWND window_handle, UINT message, WPARAM wparam, LPARAM lparam)
  {
    LRESULT result = 0;

    switch (message)
    {
    case WM_DESTROY:
      PostQuitMessage(0);
      break;

    default:
      result = DefWindowProc(window_handle, message, wparam, lparam);
      break;
    }
    
    return result;
  }

  void initialize_win32(void)
  {
    Scratch scratch;
    context.allocator->derive(&scratch);

    this->win32_instance = GetModuleHandle(0);
    GetStartupInfo(&this->win32_startup_info);

    const wchar_t window_class_name[] = L"M";
    WNDCLASS window_class =
    {
      .style         = CS_HREDRAW | CS_VREDRAW,
      .lpfnWndProc   = this->win32_process_window_message,
      .cbClsExtra    = 0,
      .cbWndExtra    = 0,
      .hInstance     = this->win32_instance,
      .hIcon         = 0,
      .hCursor       = 0,
      .hbrBackground = 0,
      .lpszMenuName  = 0,
      .lpszClassName = window_class_name,
    };
    RegisterClass(&window_class);
    uint wide_application_name_length;
    utf16 *wide_application_name = make_terminated_utf16_string_from_utf8(&wide_application_name_length, this->application_name);
    this->win32_window = CreateWindowEx(
      WS_EX_OVERLAPPEDWINDOW,
      window_class_name,
      wide_application_name,
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      0,
      0,
      this->win32_instance,
      0);
    if (!this->win32_window) throw std::runtime_error("failed to create the main window.");
    ShowWindow(this->win32_window, this->win32_startup_info.wShowWindow);
    scratch.die();
  }

  static VKAPI_ATTR VkBool32 VKAPI_CALL vk_process_debug_message(
    VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
    VkDebugUtilsMessageTypeFlagsEXT             message_types,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void*                                       user_data)
  {
    if (message_severity < VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
      return VK_FALSE;
    
    printf("[Vulkan] ");

    switch (message_severity)
    {     
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: printf("[verbose] "); break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:    printf("[info] ");    break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: printf("[warning] "); break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:   printf("[error] ");   break;
    default: break;
    }

    if (message_types & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)                printf("[general] ");
    if (message_types & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)             printf("[validation] ");
    if (message_types & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)            printf("[performance] ");
    if (message_types & VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT) printf("[device_address_binding] ");

    printf("%s\n", callback_data->pMessage);
    return VK_FALSE;
  }

  void initialize_vulkan(void)
  {
    Scratch scratch;
    context.allocator->derive(&scratch);

  #if 0
    uint32_t extensions_count;
    vkEnumerateInstanceExtensionProperties(0, &extensions_count, 0);
    VkExtensionProperties *extensions_properties = scratch.push<VkExtensionProperties>(extensions_count);
    vkEnumerateInstanceExtensionProperties(0, &extensions_count, extensions_properties);
    for (uint i = 0; i < extensions_count; ++i)
      printf("vulkan extension: %s\n", extensions_properties[i].extensionName);
  #endif

    const char *enabled_layer_names[] =
    {
    #if defined(DEBUGGING)
      "VK_LAYER_KHRONOS_validation"
    #endif
    };
    uint enabled_layers_count = countof(enabled_layer_names);

    const char *enabled_extension_names[] =
    {
    #if defined(DEBUGGING)
      "VK_EXT_debug_utils",
    #endif
      "VK_KHR_surface",
      "VK_KHR_win32_surface",
      "VK_KHR_display",
    };
    uint enabled_extensions_count = countof(enabled_extension_names);

    /* create an instance */
    {
      VkApplicationInfo application_info =
      {
        .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext              = 0,
        .pApplicationName   = this->application_name,
        .applicationVersion = this->application_version,
        .pEngineName        = this->application_name,
        .engineVersion      = this->application_version,
        .apiVersion         = VK_API_VERSION_1_0,
      };

      VkInstanceCreateInfo instance_creation_info =
      {
        .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext                   = 0,
        .flags                   = 0,
        .pApplicationInfo        = &application_info,
        .enabledLayerCount       = enabled_layers_count,
        .ppEnabledLayerNames     = enabled_layer_names,
        .enabledExtensionCount   = enabled_extensions_count,
        .ppEnabledExtensionNames = enabled_extension_names,
      };

    #if defined(DEBUGGING)
      VkDebugUtilsMessengerCreateInfoEXT debug_messenger_creation_info =
      {
        .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .pNext           = 0,
        .flags           = 0,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,
        .pfnUserCallback = vk_process_debug_message,
        .pUserData       = 0,
      };
      instance_creation_info.pNext = &debug_messenger_creation_info;
    #endif

      VkResult result = vkCreateInstance(&instance_creation_info, 0, &this->vk_instance);
      if (result != VK_SUCCESS) throw std::runtime_error("failed to create an instance for Vulkan.");

    #if defined(DEBUGGING)   
      auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(this->vk_instance, "vkCreateDebugUtilsMessengerEXT");
      if (!vkCreateDebugUtilsMessengerEXT) throw std::runtime_error("failed to get `vkCreateDebugUtilsMessengerEXT`.");
      result = vkCreateDebugUtilsMessengerEXT(this->vk_instance, &debug_messenger_creation_info, 0, &this->vk_debug_messenger);
      if (result != VK_SUCCESS) throw std::runtime_error("failed to create a debug messenger for Vulkan.");
    #endif
    }

    /* create a surface */
    {
      VkWin32SurfaceCreateInfoKHR surface_creation_info =
      {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .pNext = 0,
        .flags = 0,
        .hinstance = this->win32_instance,
        .hwnd      = this->win32_window,
      };
      VkResult result = vkCreateWin32SurfaceKHR(this->vk_instance, &surface_creation_info, 0, &this->vk_surface);
      if (result != VK_SUCCESS) throw std::runtime_error("failed to create a surface for Vulkan.");
    }

    const char *physical_device_extension_names[] =
    {
      "VK_KHR_swapchain",
    };
    uint physical_device_extensions_count = countof(physical_device_extension_names);

    constexpr uint queue_stuff_family_indexes_count = 2; 
    struct Queue_Stuff
    {
      union
      {
        uint32_t indexes[queue_stuff_family_indexes_count];
        struct
        {
          uint32_t graphics_family_index;
          uint32_t presentation_family_index;
        };
      };

      bool has_graphics_family : 1 = false;
      bool has_presentation_family : 1 = false;
    } queue_stuff;

    struct Swapchain_Details
    {
      VkSurfaceCapabilitiesKHR capabilities;
      uint formats_count;
      VkSurfaceFormatKHR *formats;
      uint modes_count;
      VkPresentModeKHR *modes;
      VkSurfaceFormatKHR format;
      VkPresentModeKHR   mode; 
      VkExtent2D         extent;
      uint               images_count;
    } swapchain_details;

    /* find a suitable physical device */
    {
      uint32_t physical_devices_count;
      vkEnumeratePhysicalDevices(this->vk_instance, &physical_devices_count, 0);
      if (!physical_devices_count) throw std::runtime_error("failed to enumerate physical devices for Vulkan");
      VkPhysicalDevice *physical_devices = scratch.push<VkPhysicalDevice>(physical_devices_count);
      vkEnumeratePhysicalDevices(this->vk_instance, &physical_devices_count, physical_devices);

      uint highest_score = 0;
      for (uint32_t i = 0; i < physical_devices_count; ++i)
      {
        VkPhysicalDevice           physical_device = physical_devices[i];
        VkPhysicalDeviceProperties physical_device_properties;
        VkPhysicalDeviceFeatures   physical_device_features;
        vkGetPhysicalDeviceProperties(physical_device, &physical_device_properties);
        vkGetPhysicalDeviceFeatures(physical_device, &physical_device_features);

        uint score = 0;
        bool suitable = physical_device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && physical_device_features.geometryShader;
        if (!suitable) continue;

        /* check supporting extensions */
        {
          uint32_t extensions_count;
          vkEnumerateDeviceExtensionProperties(physical_device, 0, &extensions_count, 0);
          VkExtensionProperties *extensions_properties = scratch.push<VkExtensionProperties>(extensions_count);
          vkEnumerateDeviceExtensionProperties(physical_device, 0, &extensions_count, extensions_properties);
          for (uint i = 0; i < physical_device_extensions_count; ++i)
          {
            bool found = false;
            for (uint j = 0; j < extensions_count; ++j)
            {
              if (compare_string(physical_device_extension_names[i], extensions_properties[j].extensionName))
              {
                found = true;
                break;
              }
            }
            if (!found)
            {
              suitable = false;
              break;
            }
          }
        }
        if (!suitable) continue;

        /* find queue families */
        {
          uint32_t queue_families_count;
          vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, 0);
          VkQueueFamilyProperties *queue_families_properties = scratch.push<VkQueueFamilyProperties>(queue_families_count);
          vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, queue_families_properties);

          for (uint32_t i = 0; i < queue_families_count; ++i)
          {
            VkQueueFamilyProperties *properties = queue_families_properties + i;
            if (!queue_stuff.has_graphics_family && properties->queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
              queue_stuff.graphics_family_index = i;
              queue_stuff.has_graphics_family = true;
            }
            if (!queue_stuff.has_presentation_family)
            {
              VkBool32 supports_presentation;
              vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, this->vk_surface, &supports_presentation);
              if (supports_presentation)
              {
                queue_stuff.presentation_family_index = i;
                queue_stuff.has_presentation_family = true;
              }
            }
          }
          bool has_all_indexes = queue_stuff.has_graphics_family && queue_stuff.has_presentation_family;
          if (!has_all_indexes) suitable = false;
        }
        if (!suitable) continue;

        /* check swapchain details */   
        {
          /* check capabilities */
          {
            VkSurfaceCapabilitiesKHR *capabilities = &swapchain_details.capabilities;
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, this->vk_surface, capabilities);
          #if 0
            printf("swapchain_details.capabilities:\n");
            printf("\tminImageCount: %u\n",       capabilities->minImageCount);
            printf("\tmaxImageCount: %u\n",       capabilities->maxImageCount);
            printf("\tcurrentExtent:\n");
            printf("\t\twidth: %u\n",             capabilities->currentExtent.width);
            printf("\t\theight: %u\n",            capabilities->currentExtent.height);
            printf("\tminImageExtent:\n");
            printf("\t\twidth: %u\n",             capabilities->minImageExtent.width);
            printf("\t\theight: %u\n",            capabilities->minImageExtent.height);
            printf("\tmaxImageExtent:\n");
            printf("\t\twidth: %u\n",             capabilities->maxImageExtent.width);
            printf("\t\theight: %u\n",            capabilities->maxImageExtent.height);
            printf("\tmaxImageArrayLayers: %u\n", capabilities->maxImageArrayLayers);
          #endif
            if (capabilities->currentExtent.width != uint32_maximum)
              swapchain_details.extent = capabilities->currentExtent;
            else
            {
              RECT rect;
              GetClientRect(this->win32_window, &rect);
              swapchain_details.extent =
              {
                .width  = clamp(rect.right - rect.left, capabilities->minImageExtent.width, capabilities->maxImageExtent.width),
                .height = clamp(rect.bottom - rect.top, capabilities->minImageExtent.height, capabilities->maxImageExtent.height),
              };
            }
            swapchain_details.images_count = swapchain_details.capabilities.minImageCount + 1;
          #if 0
            printf("swapchain_details.extent:\n");
            printf("\twidth: %u\n", swapchain_details.extent.width);
            printf("\theight: %u\n", swapchain_details.extent.height);
            printf("swapchain_details.images_count: %u\n", swapchain_details.images_count);
          #endif
          }

          /* check and set format */
          {
            vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, this->vk_surface, &swapchain_details.formats_count, 0);
            if (swapchain_details.formats_count)
            {
              swapchain_details.formats = scratch.push<VkSurfaceFormatKHR>(swapchain_details.formats_count);
              vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, this->vk_surface, &swapchain_details.formats_count, swapchain_details.formats);
            }
            bool found_format = false;
            for (uint i = 0; i < swapchain_details.formats_count; ++i)
            {
              VkSurfaceFormatKHR *format = swapchain_details.formats + i;
            #if 0
              printf("swapchain_details.formats[%u]:\n", i);
              printf("\tformat: %i\n", format->format);
              printf("\tcolorSpace: %i\n", format->colorSpace);
            #endif
              if (format->format == VK_FORMAT_B8G8R8A8_SRGB && format->colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
              {
                found_format = true;
                swapchain_details.format = *format;
                break;
              }
            }
            if (!found_format) suitable = false;
          }
          if (!swapchain_details.formats_count)suitable = false;

          /* set mode */
          {
            vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, this->vk_surface, &swapchain_details.modes_count, 0);
            if (swapchain_details.modes_count)
            {
              swapchain_details.modes = scratch.push<VkPresentModeKHR>(swapchain_details.modes_count);
              vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, this->vk_surface, &swapchain_details.modes_count, swapchain_details.modes);
            }
            bool found_mode = false;
            for (uint i = 0; i < swapchain_details.modes_count; ++i)
            {
              VkPresentModeKHR *mode = swapchain_details.modes + i;
            #if 0
              printf("swapchain_details.modes[%u]: %i\n", i, *mode);
            #endif

            #if 0
              if (*mode == VK_PRESENT_MODE_MAILBOX_KHR)
              {
                found_mode = true;
                swapchain_details.mode = *mode;
                break;
              }
            #endif
            }
            if (!found_mode) swapchain_details.mode = VK_PRESENT_MODE_FIFO_KHR;
          }
          if (!swapchain_details.modes_count) suitable = false;
        }
        if (!suitable) continue;

        if (this->vk_physical_device && score <= highest_score) continue;
        highest_score = score;
        this->vk_physical_device = physical_device;
      }

      if (!this->vk_physical_device) std::runtime_error("failed to find a suitable physical device for Vulkan.");
    }

  #if 0
    uint32_t displays_count;
    vkGetPhysicalDeviceDisplayPropertiesKHR(this->vk_physical_device, &displays_count, 0);
    VkDisplayPropertiesKHR *displays_properties = scratch.push<VkDisplayPropertiesKHR>(displays_count);
    for (uint i = 0; i < displays_count; ++i)
    {
      VkDisplayPropertiesKHR *display_properties = displays_properties + i;
      printf("vulkan display: %s\n", display_properties->displayName);
      printf("\tdimensions: %u %u\n", display_properties->physicalDimensions.width, display_properties->physicalDimensions.height);
      printf("\tresolution: %u %u\n", display_properties->physicalResolution.width, display_properties->physicalResolution.height);
    }
  #endif
    
    /* create a logical device */
    {
      float priority = 1.f;
      uint queue_creation_infos_count = 0;
      VkDeviceQueueCreateInfo *queue_creation_infos = scratch.push<VkDeviceQueueCreateInfo>(queue_stuff_family_indexes_count);
      for (uint i = 0; i < queue_stuff_family_indexes_count; ++i)
      {
        uint32_t family_index = queue_stuff.indexes[i];
        bool unique = true;
        for (uint j = 0; j < queue_creation_infos_count; ++j)
        {
          if (family_index == queue_creation_infos[j].queueFamilyIndex)
          {
            unique = false;
            break;
          }
        }
        if (unique)
        {
          queue_creation_infos[queue_creation_infos_count++] =
          {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext            = 0,
            .flags            = 0,
            .queueFamilyIndex = family_index,
            .queueCount       = 1,
            .pQueuePriorities = &priority,
          };
        }
      }
      VkPhysicalDeviceFeatures physical_device_features =
      {
      };
      VkDeviceCreateInfo device_creation_info =
      {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .queueCreateInfoCount    = queue_creation_infos_count,
        .pQueueCreateInfos       = queue_creation_infos,
        .enabledLayerCount       = enabled_layers_count,
        .ppEnabledLayerNames     = enabled_layer_names,
        .enabledExtensionCount   = physical_device_extensions_count,
        .ppEnabledExtensionNames = physical_device_extension_names,
        .pEnabledFeatures        = &physical_device_features,
      };
      VkResult result = vkCreateDevice(this->vk_physical_device, &device_creation_info, 0, &this->vk_device);
      if (result != VK_SUCCESS) throw std::runtime_error("failed to create a device for Vulkan.");
    }

    /* get queues */
    {
      vkGetDeviceQueue(this->vk_device, queue_stuff.graphics_family_index, 0, &this->vk_graphics_queue);
      vkGetDeviceQueue(this->vk_device, queue_stuff.presentation_family_index, 0, &this->vk_presentation_queue);
    }

    /* create a swapchain */
    {
      VkSwapchainCreateInfoKHR swapchain_creation_info =
      {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = 0,
        .flags = 0,
        .surface          = this->vk_surface,
        .minImageCount    = swapchain_details.images_count,
        .imageFormat      = swapchain_details.format.format,
        .imageColorSpace  = swapchain_details.format.colorSpace,
        .imageExtent      = swapchain_details.extent,
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        /* */
        .preTransform     = swapchain_details.capabilities.currentTransform,
        .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode      = swapchain_details.mode,
        .clipped          = VK_TRUE,
        .oldSwapchain     = 0,
      };
      if (queue_stuff.graphics_family_index != queue_stuff.presentation_family_index)
      {
        swapchain_creation_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        swapchain_creation_info.queueFamilyIndexCount = queue_stuff_family_indexes_count;
        swapchain_creation_info.pQueueFamilyIndices   = queue_stuff.indexes;
      }
      else
      {
        swapchain_creation_info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
        swapchain_creation_info.queueFamilyIndexCount = 0;
        swapchain_creation_info.pQueueFamilyIndices   = 0;
      }
      VkResult result = vkCreateSwapchainKHR(this->vk_device, &swapchain_creation_info, 0, &this->vk_swapchain);
      if (result != VK_SUCCESS) throw std::runtime_error("failed to create a swapchain for Vulkan.");
    }

    scratch.die();
  }
  
  void initialize(void)
  {
    /* initialize modules */
    {
      this->initialize_win32();
      this->initialize_vulkan();
    }
  }

  void terminate_vulkan(void)
  {
    vkDestroySwapchainKHR(this->vk_device, this->vk_swapchain, 0);
    vkDestroySurfaceKHR(this->vk_instance, this->vk_surface, 0);
    vkDestroyDevice(this->vk_device, 0);
  #if defined(DEBUGGING) 
    auto vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(this->vk_instance, "vkDestroyDebugUtilsMessengerEXT");
    if (vkDestroyDebugUtilsMessengerEXT) vkDestroyDebugUtilsMessengerEXT(this->vk_instance, this->vk_debug_messenger, 0);
  #endif
    vkDestroyInstance(this->vk_instance, 0);
  }

  void terminate(void)
  {
  }
};

static Breakout breakout;

int main(void)
{
  try
  {
    breakout.execute();
  }
  catch (const std::exception &e)
  {
    MessageBoxA(0, e.what(), 0, 0);
    return EXIT_FAILURE;
  }

  printf("goodbye!");
  return EXIT_SUCCESS;
}

/******************************************************************************/

inline uint get_backward_alignment(Address address, uint alignment)
{
  return alignment ? address & (alignment - 1) : 0;
}

inline uint get_forward_alignment(Address address, uint alignment)
{
  uint modulus = get_backward_alignment(address, alignment);
  return modulus ? alignment - modulus : 0;
}

inline uint get_maximum(uint left, uint right)
{
  return left >= right ? left : right;
}

inline uint get_minimum(uint left, uint right)
{
  return left <= right ? left : right;
}

uint clamp(uint value, uint minimum, uint maximum)
{
  return value > maximum ? maximum : value < minimum ? minimum : value;
}

inline void fill_memory(byte value, void *memory, uint size)
{
  memset(memory, value, size);
}

inline uint get_string_length(const char *string)
{
  return strlen(string);
}

inline sint compare_string(const utf8 *left, const utf8 *right)
{
  return strcmp(left, right);
}

utf16 *make_terminated_utf16_string_from_utf8(uint *utf16_string_length, const utf8 *utf8_string)
{
  int possibly_utf16_string_length = MultiByteToWideChar(CP_UTF8, 0, utf8_string, get_string_length(utf8_string), 0, 0);
  assert(possibly_utf16_string_length >= 0);
  *utf16_string_length = possibly_utf16_string_length + 1;
  utf16 *utf16_string = context.allocator->push<utf16>(*utf16_string_length);
  assert(MultiByteToWideChar(CP_UTF8, 0, utf8_string, get_string_length(utf8_string), utf16_string, *utf16_string_length) >= 0);
  return utf16_string;
}

thread_local Context context;

#if 0 /* excluded since it's useless ATM */

template<typename T>
void Array<T>::initialize(uint capacity)
{
  this->items    = 0;
  this->capacity = 0;
  this->count    = 0;
  this->ensure_capacity(capacity);
}

template<typename T>
uint Array<T>::space(void)
{
  return this->capacity - this->count;
}

template<typename T>
T *Array<T>::get(uint index)
{
  return this->items + index;
}

template<typename T>
T *Array<T>::push(uint count)
{
  this->ensure_capacity(count);
  T *result = this->items + this->count;
  this->count += count;
  return result;
}

template<typename T>
void Array<T>::pop(uint count)
{
  if (count > this->count) count = this->count;
  this->count -= count;
}

template<typename T>
void Array<T>::reallocate(uint count)
{
  this->items = (T *)context.allocator->reallocate(this->items, this->count * sizeof(T), count * sizeof(T), alignof(T));
}

template<typename T>
void Array<T>::ensure_capacity(uint count)
{
  if (count > this->space())
  {
    this->capacity += count + this->capacity / 2;
    this->reallocate(this->capacity);
  }
}

#endif

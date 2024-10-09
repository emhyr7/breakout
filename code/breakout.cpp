#include "breakout.h"

#include "breakout_allocators.cpp"

class Breakout
{
public:
  void execute(void)
  {
    LARGE_INTEGER beginning_time, ending_time;
    LARGE_INTEGER clock_frequency;
    uintl elapsed_time = 0;
    QueryPerformanceFrequency(&clock_frequency);

    QueryPerformanceCounter(&beginning_time);

    this->initialize();
    ShowWindow(this->win32_window, this->win32_startup_info.wShowWindow);

    uint frames_count = 0;

    /* the main loop */
    Scratch scratch;
    context.allocator->derive(&scratch);
    for (;;)
    {
      QueryPerformanceCounter(&ending_time);
      elapsed_time += (ending_time.QuadPart - beginning_time.QuadPart) * 1000000 / clock_frequency.QuadPart;
      beginning_time = ending_time;
      ++frames_count;
      if (elapsed_time >= 1000000)
      {
        printf("FPS: %u\n", frames_count);
        frames_count = 0;
        elapsed_time = 0;
      }
      
      scratch.die();

      /* retrieve and process window messages */
      while (PeekMessage(&this->win32_window_message, 0, 0, 0, PM_REMOVE))
      {
        TranslateMessage(&this->win32_window_message);
        DispatchMessage(&this->win32_window_message);
      }
      if (this->requested_quit) break;

      this->vk_draw_frame();

    #if 0
      if (this->window_resized)
      {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(this->vk_physical_device, this->vk_surface, &capabilities);
        
        if (capabilities.currentExtent.width != uint32_maximum)
          this->vk_swapchain_extent = capabilities.currentExtent;
        else
        {
          RECT rect;
          GetClientRect(this->win32_window, &rect);
          this->vk_swapchain_extent =
          {
            .width  = clamp(rect.right - rect.left, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            .height = clamp(rect.bottom - rect.top, capabilities.minImageExtent.height, capabilities.maxImageExtent.height),
          };
        }
        printf("this->vk_swapchain_extent:\n");
        printf("\twidth: %u\n", this->vk_swapchain_extent.width);
        printf("\theight: %u\n", this->vk_swapchain_extent.height);
      }
    #endif
    }

    vkDeviceWaitIdle(this->vk_device);

    this->terminate();
  }

private:
  static constexpr char     application_name[]  =  "BREAKOUT";
  static constexpr uint32_t application_version = VK_MAKE_VERSION(1, 0, 0);

  /* general fields */
  Linear_Allocator persistent_allocator;
  bool requested_quit : 1 = 0;

  /* Win32 fields */
  HINSTANCE    win32_instance;
  STARTUPINFOW win32_startup_info;
  HWND         win32_window;
  MSG          win32_window_message;

  /* Vulkan fields */
  VkInstance               vk_instance;
  VkDebugUtilsMessengerEXT vk_debug_messenger;
  VkSurfaceKHR             vk_surface;
  VkPhysicalDevice         vk_physical_device;
  VkDevice                 vk_device;
  VkQueue                  vk_graphics_queue;
  uint32_t                 vk_graphics_queue_family_index;
  VkQueue                  vk_presentation_queue;
  uint32_t                 vk_presentation_queue_family_index;
  VkSwapchainKHR           vk_swapchain;
  VkImage                 *vk_swapchain_images;
  VkImageView             *vk_swapchain_image_views;
  uint                     vk_swapchain_images_count;
  uint                     vk_swapchain_images_capacity;
  uint                     vk_swapchain_image_layers_count;
  VkFormat                 vk_swapchain_format;
  VkExtent2D               vk_swapchain_extent;
  VkRenderPass             vk_render_pass;
  VkShaderModule           vk_basic_vert_shader;
  VkShaderModule           vk_basic_frag_shader;
  VkPipelineLayout         vk_graphics_pipeline_layout;
  VkPipeline               vk_graphics_pipeline;
  VkFramebuffer           *vk_swapchain_framebuffers;
  VkCommandPool            vk_command_pool;
  VkCommandBuffer         *vk_command_buffers;
  uint                     vk_command_buffers_count;
  VkSemaphore              vk_image_availability_semaphore;
  VkSemaphore              vk_rendering_finality_semaphore;
  VkFence                  vk_in_flight_fence;


/******************************************************************************/
  
  static LRESULT CALLBACK win32_process_window_message(HWND window_handle, UINT message, WPARAM wparam, LPARAM lparam)
  {
    LRESULT result = 0;
    Breakout *self = (Breakout *)GetWindowLongPtr(window_handle, GWLP_USERDATA);

    switch (message)
    {
    case WM_CREATE:
      {
        const CREATESTRUCT *creation_info = (const CREATESTRUCT *)lparam;
        SetWindowLongPtr(window_handle, GWLP_USERDATA, (LONG_PTR)creation_info->lpCreateParams);
        break;
      }
    case WM_DESTROY:
    case WM_QUIT:
    quit:
      self->requested_quit = true;
      break;

    case WM_KEYDOWN:
      if (wparam == VK_ESCAPE) goto quit;
      break;

    case WM_SIZE:
    case WM_PAINT:
      result = DefWindowProc(window_handle, message, wparam, lparam);
      break;

    case WM_ENTERSIZEMOVE:
      break;
    case WM_EXITSIZEMOVE:
      break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:

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
    WNDCLASSEX window_class =
    {
      .cbSize        = sizeof(window_class),
      .style         = 0,
      .lpfnWndProc   = this->win32_process_window_message,
      .cbClsExtra    = 0,
      .cbWndExtra    = 0,
      .hInstance     = this->win32_instance,
      .hIcon         = 0,
      .hCursor       = 0,
      .hbrBackground = 0,
      .lpszMenuName  = 0,
      .lpszClassName = window_class_name,
      .hIconSm       = 0,
    };
    RegisterClassEx(&window_class);
    uint wide_application_name_length;
    utf16 *wide_application_name = make_terminated_utf16_string_from_utf8(&wide_application_name_length, this->application_name);
    this->win32_window = CreateWindowEx(
      WS_EX_OVERLAPPEDWINDOW,
      window_class_name,
      wide_application_name,
      WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      0,
      0,
      this->win32_instance,
      this);
    if (!this->win32_window) throw std::runtime_error("failed to create the main window.");
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

  #if 0
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
  #endif

    printf("%s\n", callback_data->pMessage);
    return VK_FALSE;
  }

  VkShaderModule vk_load_shader(const char *path)
  {
    Context prior_context = context;
    context.linear_allocator = &this->persistent_allocator;
    Span<uintb> data = read_from_file_quickly(path);
    context = prior_context;

    VkShaderModuleCreateInfo creation_info =
    {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .pNext = 0,
      .flags = 0,
      .codeSize = data.count,
      .pCode    = (uint32_t *)data.items,
    };
    VkShaderModule shader;
    if (vkCreateShaderModule(this->vk_device, &creation_info, 0, &shader) != VK_SUCCESS) throw std::runtime_error("Failed to create a shader module for Vulkan.");
    return shader;
  }

  void vk_record_command_buffer(VkCommandBuffer command_buffer, uint image_index)
  {
    VkCommandBufferBeginInfo beginning_info =
    {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = 0,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = 0,
    };
    if (vkBeginCommandBuffer(command_buffer, &beginning_info) != VK_SUCCESS) throw std::runtime_error("Failed to begin recording a command buffer for Vulkan.");

    VkClearValue clear_values[] =
    {
      {{{ 0.0f, 0.0f, 0.0f, 0.0f }}},
    };
    constexpr uint clear_values_count = countof(clear_values);
    
    VkRenderPassBeginInfo render_pass_beginning_info =
    {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .pNext = 0,
      .renderPass  = this->vk_render_pass,
      .framebuffer = this->vk_swapchain_framebuffers[image_index],
      .renderArea =
      {
        .offset = { 0, 0 },
        .extent = this->vk_swapchain_extent,
      },
      .clearValueCount = clear_values_count,
      .pClearValues    = clear_values,
    };
    vkCmdBeginRenderPass(command_buffer, &render_pass_beginning_info, VK_SUBPASS_CONTENTS_INLINE);
    {
      vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, this->vk_graphics_pipeline);
      
      VkViewport viewports[] =
      {
        {
          .x        = 0.0f,
          .y        = 0.0f,
          .width    = (float)this->vk_swapchain_extent.width,
          .height   = (float)this->vk_swapchain_extent.height,
          .minDepth = 0.0f,
          .maxDepth = 1.0f,
        },
      };
      constexpr uint viewports_count = countof(viewports);
      vkCmdSetViewport(command_buffer, 0, viewports_count, viewports);

      VkRect2D scissors[] =
      {
        {
          .offset = { 0, 0 },
          .extent = this->vk_swapchain_extent,
        }
      };
      constexpr uint scissors_count = countof(scissors);
      vkCmdSetScissor(command_buffer, 0, scissors_count, scissors);

      constexpr uint vertexes_count = 3; /* hardcoded in the vertex shader */
      constexpr uint instances_count = 1; /* idk what this is */
      vkCmdDraw(command_buffer, vertexes_count, instances_count, 0, 0);
    }
    vkCmdEndRenderPass(command_buffer);
    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) throw std::runtime_error("Failed to end a command buffer for Vulkan");
  }

  void vk_draw_frame(void)
  {
    vkWaitForFences(this->vk_device, 1, &this->vk_in_flight_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(this->vk_device, 1, &this->vk_in_flight_fence);

    uint32_t image_index;
    vkAcquireNextImageKHR(this->vk_device, this->vk_swapchain, UINT64_MAX, this->vk_image_availability_semaphore, VK_NULL_HANDLE, &image_index);

    VkCommandBuffer command_buffer = this->vk_command_buffers[0];

    vkResetCommandPool(this->vk_device, this->vk_command_pool, 0);
    this->vk_record_command_buffer(command_buffer, image_index);

    /* wait for the color output of the first render pass before signaling the rendering finality semaphore */
    {
      VkSemaphore waiting_semaphores[] = { this->vk_image_availability_semaphore };
      constexpr uint waiting_semaphores_count = countof(waiting_semaphores);
      VkPipelineStageFlags waiting_destination_stage_mask [] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
      VkSemaphore signaling_semaphores[] = { this->vk_rendering_finality_semaphore };
      constexpr uint signaling_semaphores_count = countof(signaling_semaphores);
      VkSubmitInfo submission_info =
      {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = 0,
        .waitSemaphoreCount   = waiting_semaphores_count,
        .pWaitSemaphores      = waiting_semaphores,
        .pWaitDstStageMask    = waiting_destination_stage_mask,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &command_buffer,
        .signalSemaphoreCount = signaling_semaphores_count,
        .pSignalSemaphores    = signaling_semaphores,
      };
      if (vkQueueSubmit(this->vk_graphics_queue, 1, &submission_info, this->vk_in_flight_fence) != VK_SUCCESS) throw std::runtime_error("Failed to submit to queue for Vulkan.");     

      VkSwapchainKHR swapchains[] = { this->vk_swapchain };
      constexpr uint swapchains_count = countof(swapchains);
      
      VkPresentInfoKHR presentation_info =
      {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = 0,
        .waitSemaphoreCount = signaling_semaphores_count,
        .pWaitSemaphores    = signaling_semaphores,
        .swapchainCount     = swapchains_count,
        .pSwapchains        = swapchains,
        .pImageIndices      = &image_index,
        .pResults           = 0,
      };
      vkQueuePresentKHR(this->vk_presentation_queue, &presentation_info);
    }
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
      /* VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME, */
    #if defined(DEBUGGING)
      VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    #endif
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
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
        .apiVersion         = VK_API_VERSION_1_3,
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
        .pUserData       = this,
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

      /* store swapchain info */
      this->vk_swapchain_images_capacity = swapchain_details.capabilities.maxImageCount;
      this->vk_swapchain_format          = swapchain_details.format.format;
      this->vk_swapchain_extent          = swapchain_details.extent;
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
    
      this->vk_graphics_queue_family_index     = queue_stuff.graphics_family_index;
      this->vk_presentation_queue_family_index = queue_stuff.presentation_family_index;

      /* get queues */
      vkGetDeviceQueue(this->vk_device, queue_stuff.graphics_family_index, 0, &this->vk_graphics_queue);
      vkGetDeviceQueue(this->vk_device, queue_stuff.presentation_family_index, 0, &this->vk_presentation_queue);
    }

    /* create a swapchain */
    {
      this->vk_swapchain_image_layers_count = 1;
      VkSwapchainCreateInfoKHR swapchain_creation_info =
      {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = 0,
        .flags = 0,
        .surface          = this->vk_surface,
        .minImageCount    = swapchain_details.images_count + 1,
        .imageFormat      = swapchain_details.format.format,
        .imageColorSpace  = swapchain_details.format.colorSpace,
        .imageExtent      = swapchain_details.extent,
        .imageArrayLayers = this->vk_swapchain_image_layers_count,
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

      /* get images */
      this->vk_swapchain_images = this->persistent_allocator.push<VkImage>(this->vk_swapchain_images_capacity);
      vkGetSwapchainImagesKHR(this->vk_device, this->vk_swapchain, &this->vk_swapchain_images_count, 0);
      vkGetSwapchainImagesKHR(this->vk_device, this->vk_swapchain, &this->vk_swapchain_images_count, this->vk_swapchain_images);

      /* get image views */
      this->vk_swapchain_image_views = this->persistent_allocator.push<VkImageView>(this->vk_swapchain_images_capacity);
      for (uint i = 0; i < this->vk_swapchain_images_count; ++i)
      {
        VkImageViewCreateInfo creation_info =
        {
          .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          .pNext = 0,
          .flags = 0,
          .image            = this->vk_swapchain_images[i],
          .viewType         = VK_IMAGE_VIEW_TYPE_2D,
          .format           = this->vk_swapchain_format,
          .components       =
          {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
          },
          .subresourceRange =
          {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1,
          },
        };
        VkResult result = vkCreateImageView(this->vk_device, &creation_info, 0, &this->vk_swapchain_image_views[i]);
        if (result != VK_SUCCESS) throw std::runtime_error("failed to create an image view for Vulkan.");
      }
    }

    /* create render passes */
    {
      VkAttachmentDescription color_attachments[] =
      {
        {
          .flags          = 0,
          .format         = this->vk_swapchain_format,
          .samples        = VK_SAMPLE_COUNT_1_BIT,
          .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR, /* clear the framebuffer to black per frame */
          .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
          .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
          .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
          .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
          .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        },
      };
      constexpr uint color_attachments_count = countof(color_attachments);

      VkAttachmentReference color_attachment_references[] =
      {
        {
          .attachment = 0,
          .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        },
      };
      constexpr uint color_attachment_references_count = countof(color_attachment_references);

      VkSubpassDescription subpass_descriptions[] =
      {
        {
          .flags                   = 0,
          .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
          .inputAttachmentCount    = 0,
          .pInputAttachments       = 0,
          .colorAttachmentCount    = color_attachment_references_count,
          .pColorAttachments       = color_attachment_references,
          .pResolveAttachments     = 0,
          .pDepthStencilAttachment = 0,
          .preserveAttachmentCount = 0,
          .pPreserveAttachments    = 0,
        },
      };
      constexpr uint subpasses_count = countof(subpass_descriptions);

      VkSubpassDependency dependencies[] =
      {
        {
          .srcSubpass      = VK_SUBPASS_EXTERNAL,
          .dstSubpass      = 0,
          .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
          .srcAccessMask   = 0,
          .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
          .dependencyFlags = 0,
        },
      };
      constexpr uint dependencies_count = countof(dependencies);

      VkRenderPassCreateInfo creation_info =
      {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .attachmentCount = color_attachments_count,
        .pAttachments    = color_attachments,
        .subpassCount    = subpasses_count,
        .pSubpasses      = subpass_descriptions,
        .dependencyCount = dependencies_count,
        .pDependencies   = dependencies,
      };
      if (vkCreateRenderPass(this->vk_device, &creation_info, 0, &this->vk_render_pass) != VK_SUCCESS) throw std::runtime_error("Failed to create a render pass for Vulkan.");
    }

    /* create the pipeline */
    {
      this->vk_basic_vert_shader = this->vk_load_shader("data/basic.vert.spv");
      this->vk_basic_frag_shader = this->vk_load_shader("data/basic.frag.spv");

      VkPipelineShaderStageCreateInfo shader_stage_creation_infos[] =
      {
        {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext = 0,
          .flags = 0,
          .stage               = VK_SHADER_STAGE_VERTEX_BIT,
          .module              = this->vk_basic_vert_shader,
          .pName               = "main",
          .pSpecializationInfo = 0,
        },
        {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext = 0,
          .flags = 0,
          .stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module              = this->vk_basic_frag_shader,
          .pName               = "main",
          .pSpecializationInfo = 0,
        },
      };
      constexpr uint shader_stages_count = countof(shader_stage_creation_infos);

      VkPipelineVertexInputStateCreateInfo vertex_input_state_creation_info =
      {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .vertexBindingDescriptionCount   = 0,
        .pVertexBindingDescriptions      = 0,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions    = 0,
      };

      VkPipelineInputAssemblyStateCreateInfo input_assembly_state_creation_info =
      {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
      };

      VkViewport viewports[] =
      {
        {
          .x       = 0.0f,
          .y       = 0.0f,
          .width   = (float)this->vk_swapchain_extent.width,
          .height  = (float)this->vk_swapchain_extent.height,
          .minDepth = 0.0f,
          .maxDepth = 1.0f,
        },
      };

      VkRect2D scissors[] =
      {
        {
          .offset = {0, 0},
          .extent = this->vk_swapchain_extent,
        },
      };

      VkPipelineViewportStateCreateInfo viewport_state_creation_info =
      {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .viewportCount = countof(viewports),
        .pViewports    = viewports,
        .scissorCount  = countof(scissors),
        .pScissors     = scissors,
      };

      VkPipelineRasterizationStateCreateInfo rasterization_state_creation_info =
      {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .depthClampEnable        = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode             = VK_POLYGON_MODE_FILL,
        .cullMode                = VK_CULL_MODE_BACK_BIT,
        .frontFace               = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable         = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp          = 0.0f,
        .depthBiasSlopeFactor    = 0.0f,
        .lineWidth               = 1.0f,
      };

      VkPipelineMultisampleStateCreateInfo multisample_state_creation_info =
      {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable   = VK_FALSE,
        .minSampleShading      = 1.0f,
        .pSampleMask           = 0,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable      = VK_FALSE,
      };

      VkPipelineColorBlendAttachmentState color_blend_attachment_states[] =
      {
        {
          .blendEnable         = VK_FALSE,
          .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
          .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
          .colorBlendOp        = VK_BLEND_OP_ADD,
          .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
          .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
          .alphaBlendOp        = VK_BLEND_OP_ADD,
          .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT |  VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        },
      };

      VkPipelineColorBlendStateCreateInfo color_blend_state_creation_info =
      {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .logicOpEnable   = VK_FALSE,
        .logicOp         = VK_LOGIC_OP_COPY,
        .attachmentCount = countof(color_blend_attachment_states),
        .pAttachments    = color_blend_attachment_states,
        .blendConstants  = { 0.0f, 0.0f, 0.0f, 0.0f },
      };

      VkDynamicState dynamic_states[] =
      {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
      };
      constexpr uint dynamic_states_count = countof(dynamic_states);
      
      VkPipelineDynamicStateCreateInfo dynamic_state_creation_info =
      {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .dynamicStateCount = dynamic_states_count,
        .pDynamicStates    = dynamic_states,
      };     

      VkPipelineLayoutCreateInfo pipeline_layout_creation_info =
      {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .setLayoutCount         = 0,
        .pSetLayouts            = 0,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges    = 0,
        
      };
      if (vkCreatePipelineLayout(this->vk_device, &pipeline_layout_creation_info, 0, &this->vk_graphics_pipeline_layout) != VK_SUCCESS) throw std::runtime_error("Failed to create a pipeline layout for Vulkan.");

      VkGraphicsPipelineCreateInfo graphics_pipeline_creation_info =
      {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .stageCount          = shader_stages_count,
        .pStages             = shader_stage_creation_infos,
        .pVertexInputState   = &vertex_input_state_creation_info,
        .pInputAssemblyState = &input_assembly_state_creation_info,
        .pViewportState      = &viewport_state_creation_info,
        .pRasterizationState = &rasterization_state_creation_info,
        .pMultisampleState   = &multisample_state_creation_info,
        .pDepthStencilState  = 0,
        .pColorBlendState    = &color_blend_state_creation_info,
        .pDynamicState       = &dynamic_state_creation_info,
        .layout              = this->vk_graphics_pipeline_layout,
        .renderPass          = this->vk_render_pass,
        .subpass             = 0,
        .basePipelineHandle  = VK_NULL_HANDLE,
        .basePipelineIndex   = -1,
      };
      if (vkCreateGraphicsPipelines(this->vk_device, VK_NULL_HANDLE, 1, &graphics_pipeline_creation_info, 0, &this->vk_graphics_pipeline) != VK_SUCCESS) throw std::runtime_error("Failed to create a graphics pipeline for Vulkan.");
    }

    /* create framebuffers */
    {
      this->vk_swapchain_framebuffers = this->persistent_allocator.push<VkFramebuffer>(this->vk_swapchain_images_capacity);
      for (uint i = 0; i < this->vk_swapchain_images_count; ++i)
      {
        VkImageView attachments[] =
        {
          this->vk_swapchain_image_views[i],
        };
        constexpr uint attachments_count = countof(attachments);

        VkFramebufferCreateInfo creation_info =
        {
          .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
          .pNext = 0,
          .flags = 0,
          .renderPass      = this->vk_render_pass,
          .attachmentCount = attachments_count,
          .pAttachments    = attachments,
          .width           = this->vk_swapchain_extent.width,
          .height          = this->vk_swapchain_extent.height,
          .layers          = this->vk_swapchain_image_layers_count,
        };
        if (vkCreateFramebuffer(this->vk_device, &creation_info, 0, &this->vk_swapchain_framebuffers[i]) != VK_SUCCESS) throw std::runtime_error("Failed to create a framebuffer for Vulkan.");
      }
    }

    /* create the command buffer */
    {
      VkCommandPoolCreateInfo pool_creation_info =
      {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT /* VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT */,
        .queueFamilyIndex = this->vk_graphics_queue_family_index,
      };
      if (vkCreateCommandPool(this->vk_device, &pool_creation_info, 0, &this->vk_command_pool) != VK_SUCCESS) throw std::runtime_error("Failed to create a command pool for Vulkan.");

      this->vk_command_buffers_count = 1;
      this->vk_command_buffers = this->persistent_allocator.push<VkCommandBuffer>(this->vk_command_buffers_count);
      VkCommandBufferAllocateInfo buffers_allocation_info =
      {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = 0,
        .commandPool        = this->vk_command_pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = this->vk_command_buffers_count,
      };
      if (vkAllocateCommandBuffers(this->vk_device, &buffers_allocation_info, this->vk_command_buffers) != VK_SUCCESS) throw std::runtime_error("Failed to create command buffers for Vulkan.");
    }

    /* create synchronization objects */
    {
      VkSemaphoreCreateInfo semaphore_creation_info =
      {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
      };
      if (vkCreateSemaphore(this->vk_device, &semaphore_creation_info, 0, &this->vk_image_availability_semaphore) != VK_SUCCESS) throw std::runtime_error("Failed to create the image availability semaphore for Vulkan.");
      if (vkCreateSemaphore(this->vk_device, &semaphore_creation_info, 0, &this->vk_rendering_finality_semaphore) != VK_SUCCESS) throw std::runtime_error("Failed to create the rendering finality semaphore for Vulkan.");
      
      VkFenceCreateInfo fence_creation_info =
      {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = 0,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
      };
      if (vkCreateFence(this->vk_device, &fence_creation_info, 0, &this->vk_in_flight_fence) != VK_SUCCESS) throw std::runtime_error("Failed to create the in flight fence for Vulkan.");
      
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
    vkDestroySemaphore(this->vk_device, this->vk_image_availability_semaphore, 0);
    vkDestroySemaphore(this->vk_device, this->vk_rendering_finality_semaphore, 0);
    vkDestroyFence(this->vk_device, this->vk_in_flight_fence, 0);
    
    vkDestroyCommandPool(this->vk_device, this->vk_command_pool, 0);
    for (uint i = 0; i < this->vk_swapchain_images_count; ++i)
      vkDestroyFramebuffer(this->vk_device, this->vk_swapchain_framebuffers[i], 0);
    
    vkDestroyPipeline(this->vk_device, this->vk_graphics_pipeline, 0);
    vkDestroyPipelineLayout(this->vk_device, this->vk_graphics_pipeline_layout, 0);
    vkDestroyRenderPass(this->vk_device, this->vk_render_pass, 0);

    vkDestroyShaderModule(this->vk_device, this->vk_basic_vert_shader, 0);
    vkDestroyShaderModule(this->vk_device, this->vk_basic_frag_shader, 0);
    
    for (uint i = 0; i < this->vk_swapchain_images_count; ++i)
      vkDestroyImageView(this->vk_device, this->vk_swapchain_image_views[i], 0);

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
    this->terminate_vulkan();
  }
};

Breakout breakout;

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

Handle open_file(const char *path)
{
  Scratch scratch;
  context.allocator->derive(&scratch);
  uint utf16_path_length;
  const utf16 *utf16_path = make_terminated_utf16_string_from_utf8(&utf16_path_length, path);
  HANDLE handle = CreateFile(utf16_path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  if (handle == INVALID_HANDLE_VALUE) throw std::runtime_error("Failed to open file.");
  scratch.die();
  return handle;
}

uintl get_size_of_file(Handle handle)
{
  LARGE_INTEGER win32_size;
  if (!GetFileSizeEx(handle, &win32_size)) throw std::runtime_error("Failed to get file size.");
  return win32_size.QuadPart;
}

uint read_from_file(void *buffer, uint size, Handle handle)
{
  DWORD bytes_read;
  if (!ReadFile(handle, buffer, size, &bytes_read, 0)) throw std::runtime_error("Failed to read file.");
  return bytes_read;
}

Span<uintb> read_from_file_quickly( const char *path)
{
  Handle handle = open_file(path);
  Span<uintb> result;
  result.count = get_size_of_file(handle);
  result.items = context.allocator->push(result.count);
  read_from_file(result.items, result.count, handle);
  close_file(handle);
  return result;
  
}

void close_file(Handle handle)
{
  CloseHandle(handle);
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

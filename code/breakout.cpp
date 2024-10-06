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
  HINSTANCE    win32_application_instance;
  STARTUPINFOW win32_startup_info;
  HWND         win32_window_handle;
  MSG          win32_window_message;

  /* Vulkan properties */
  VkInstance               vk_instance        = 0;
  VkDebugUtilsMessengerEXT vk_debug_messenger = 0;
  VkPhysicalDevice         vk_physical_device = 0;
  VkDevice                 vk_device          = 0;

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

    this->win32_application_instance = GetModuleHandle(0);
    GetStartupInfo(&this->win32_startup_info);

    const wchar_t window_class_name[] = L"M";
    WNDCLASS window_class =
    {
      .style         = CS_HREDRAW | CS_VREDRAW,
      .lpfnWndProc   = this->win32_process_window_message,
      .cbClsExtra    = 0,
      .cbWndExtra    = 0,
      .hInstance     = this->win32_application_instance,
      .hIcon         = 0,
      .hCursor       = 0,
      .hbrBackground = 0,
      .lpszMenuName  = 0,
      .lpszClassName = window_class_name,
    };
    RegisterClass(&window_class);
    uint wide_application_name_length;
    utf16 *wide_application_name = make_terminated_utf16_string_from_utf8(&wide_application_name_length, this->application_name);
    this->win32_window_handle = CreateWindowEx(
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
      this->win32_application_instance,
      0);
    if (!this->win32_window_handle) throw std::runtime_error("failed to create the main window.");
    ShowWindow(this->win32_window_handle, this->win32_startup_info.wShowWindow);
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

      const char *enabled_layer_names[] =
      {
      #if defined(DEBUGGING)
        "VK_LAYER_KHRONOS_validation"
      #endif
      };
      unsigned enabled_layers_count = countof(enabled_layer_names);

      const char *enabled_extension_names[] =
      {
      #if defined(DEBUGGING)
        "VK_EXT_debug_utils",
      #endif
        "VK_KHR_surface",
        "VK_KHR_win32_surface"
      };
      unsigned enabled_extensions_count = countof(enabled_extension_names);

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

        /* find queue families */
        {
          uint32_t queue_families_count;
          vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, 0);
          VkQueueFamilyProperties *queue_families_properties = scratch.push<VkQueueFamilyProperties>(queue_families_count);
          vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_families_count, queue_families_properties);

          struct
          {
            uint32_t graphics;
        
            bool has_graphics : 1 = false;
          } indexes;

          for (uint32_t i = 0; i < queue_families_count; ++i)
          {
            VkQueueFamilyProperties *properties = queue_families_properties + i;
            if (!indexes.has_graphics && properties->queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
              indexes.graphics = i;
              indexes.has_graphics = true;
            }
          }

          bool has_all_indexes = indexes.has_graphics;
          if (!has_all_indexes) suitable = false;
        }

        if (suitable)
        {
          if (score <= highest_score) continue;
          highest_score = score;
          this->vk_physical_device = physical_device;
        }
      }

      if (!this->vk_physical_device) std::runtime_error("failed to find a suitable physical device for Vulkan.");
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

inline void fill_memory(byte value, void *memory, uint size)
{
  memset(memory, value, size);
}

inline uint get_string_length(const char *string)
{
  return strlen(string);
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

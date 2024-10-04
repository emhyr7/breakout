#include "breakout.h"

class Breakout
{
public: 
  void execute(void)
  {
    initialize();

    /* The *MIGHTY* "MAIN LOOP" */
    for (;;)
    {
      {
        int message_result = GetMessage(&this->win32_window_message, 0, 0, 0);
        if (!message_result) this->requested_quit = 1;
        else if (message_result < 0)
        {
          /* resolve error here */
          throw std::runtime_error("Failed to process message.");
        }
        TranslateMessage(&this->win32_window_message);
        DispatchMessage(&this->win32_window_message);
      }

      if (this->requested_quit) break;
    }

    terminate();
  }

private:
  static constexpr wchar_t  wide_application_name[] = L"BREAKOUT";
  static constexpr char     application_name[]      =  "BREAKOUT";
  static constexpr uint32_t application_version     = VK_MAKE_VERSION(1, 0, 0);

  bool requested_quit : 1 = false;

  /* Win32 properties */
  HINSTANCE    win32_application_instance;
  STARTUPINFOW win32_startup_info;
  HWND         win32_window_handle;
  MSG          win32_window_message;

  /* Vulkan properties */
  VkInstance               vk_instance;
  VkDebugUtilsMessengerEXT vk_debug_messenger;

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

  static VKAPI_ATTR VkBool32 VKAPI_CALL vk_process_debug_message(
    VkDebugUtilsMessageSeverityFlagBitsEXT      message_severity,
    VkDebugUtilsMessageTypeFlagsEXT             message_types,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void*                                       user_data)
  {
    std::cout << "[Vulkan] ";

    switch (message_severity)
    {     
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: std::cout << "[verbose] "; break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:    std::cout << "[info] ";    break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: std::cout << "[warning] "; break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:   std::cout << "[error] ";   break;
    default: break;
    }

    if (message_types & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)                std::cout << "[general] ";
    if (message_types & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)             std::cout << "[validation] ";
    if (message_types & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)            std::cout << "[performance] ";
    if (message_types & VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT) std::cout << "[device_address_binding] ";

    std::cout << callback_data->pMessage << std::endl;
    return VK_FALSE;
  }
  
  void initialize(void)
  {
    /* initialize Win32 */
    {
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
      this->win32_window_handle = CreateWindowEx(
        WS_EX_OVERLAPPEDWINDOW,
        window_class_name,
        this->wide_application_name,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        0,
        0,
        this->win32_application_instance,
        0);
      if (!this->win32_window_handle) throw std::runtime_error("Failed to create the main window.");
      ShowWindow(this->win32_window_handle, this->win32_startup_info.wShowWindow);
    }

    /* initialize Vulkan */
    {
    #if 0
      uint32_t extensions_count;
      vkEnumerateInstanceExtensionProperties(0, &extensions_count, 0);
      VkExtensionProperties *extensions = new VkExtensionProperties[extensions_count];
      vkEnumerateInstanceExtensionProperties(0, &extensions_count, extensions);
      for (uint32_t i = 0; i < extensions_count; ++i)
        std::cout << i << ": " << extensions[i].extensionName << '\n';

      uint32_t layers_count;
      vkEnumerateInstanceLayerProperties(&layers_count, 0);
      VkLayerProperties *layers = new VkLayerProperties[layers_count];
      vkEnumerateInstanceLayerProperties(&layers_count, layers);
      for (uint32_t i = 0; i < layers_count; ++i)
        std::cout << i << ": " << layers[i].layerName << '\n';
    #endif

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
        "VK_EXT_debug_report",
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
      if (result != VK_SUCCESS) throw std::runtime_error("Failed to create Vulkan instance.");

    #if defined(DEBUGGING)
      auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(this->vk_instance, "vkCreateDebugUtilsMessengerEXT");
      if (!vkCreateDebugUtilsMessengerEXT) throw std::runtime_error("Failed to get `vkCreateDebugUtilsMessengerEXT`.");
      result = vkCreateDebugUtilsMessengerEXT(this->vk_instance, &debug_messenger_creation_info, 0, &vk_debug_messenger);
      if (result != VK_SUCCESS) throw std::runtime_error("Failed to create the VUlkan debug messenger.");
    #endif
    }
  }

  void terminate(void)
  {

  #if defined(DEBUGGING) 
    auto vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(this->vk_instance, "vkDestroyDebugUtilsMessengerEXT");
    if (vkDestroyDebugUtilsMessengerEXT) vkDestroyDebugUtilsMessengerEXT(this->vk_instance, this->vk_debug_messenger, 0);
  #endif
    vkDestroyInstance(this->vk_instance, 0);
  }
};

Breakout breakout;

int main(void)
{
  Allocator allocator =
  {
    .state = 0,
    .allocate_procedure   = [](uint size, uint alignment, void *state) -> void * { return malloc(size); },
    .deallocate_procedure = [](uint size, void *memory, uint alignment, void *state) -> void { free(memory); },
    .reallocate_procedure = [](uint size, void *memory, uint alignment, uint new_size, uint new_alignment, void *state) -> void * { return realloc(memory, size); },
  };
  default_allocator = &allocator;
 
  try
  {
    breakout.execute();
  }
  catch (const std::exception &e)
  {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}


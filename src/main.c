#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#ifdef NDEBUG
#define LOG(...)
#else
#define LOG(...)                                                               \
  do {                                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    fprintf(stderr, "\n");                                                     \
  } while (0)
#endif

#define MAX_SWAPCHAIN_IMAGE_COUNT 32

struct vulkan_renderer {
  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkQueue graphics_queue;
  VkDebugUtilsMessengerEXT debug_messenger;
  VkSurfaceKHR surface;
  VkQueue present_queue;
  VkSwapchainKHR swapchain;
  VkImage swapchain_images[MAX_SWAPCHAIN_IMAGE_COUNT];
  VkFormat swapchain_image_format;
  VkExtent2D swapchain_extent;
  VkImageView swapchain_image_views[MAX_SWAPCHAIN_IMAGE_COUNT];
  VkRenderPass render_pass;
  VkPipelineLayout pipeline_layout;
  VkPipeline pipeline;
  VkFramebuffer swapchain_framebuffers[MAX_SWAPCHAIN_IMAGE_COUNT];
  uint32_t swapchain_image_count;
  bool enable_validation_layers;
};

#define MAX_EXTENSION_COUNT 256
#define MAX_ADDITIONAL_EXTENSION_COUNT 100
#define MAX_DEVICE_COUNT 48

VKAPI_ATTR VkBool32 VKAPI_CALL
vulkan_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                      VkDebugUtilsMessageTypeFlagsEXT message_type,
                      const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                      void *user_data) {
  (void)message_severity;
  (void)message_type;
  (void)user_data;
  LOG("Validation layer: %s", callback_data->pMessage);
  return VK_FALSE;
}

bool vulkan_renderer_create_instance(struct vulkan_renderer *renderer) {

  VkApplicationInfo application_info = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "vkguide",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "None",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_0};

  const char *requested_extensions[MAX_EXTENSION_COUNT] = {0};
  uint32_t requested_extension_count = 0;
  uint32_t required_instance_extension_count;
  const char *const *required_instance_extensions =
      SDL_Vulkan_GetInstanceExtensions(&required_instance_extension_count);

  assert(requested_extension_count + required_instance_extension_count <
         MAX_EXTENSION_COUNT);
  memcpy(requested_extensions, required_instance_extensions,
         required_instance_extension_count * sizeof(const char *));
  requested_extension_count += required_instance_extension_count;

  const char *additional_extensions[MAX_ADDITIONAL_EXTENSION_COUNT] = {0};
  uint32_t additional_extension_count = 0;

  if (renderer->enable_validation_layers) {
    additional_extensions[additional_extension_count++] =
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
  }

  assert(requested_extension_count + additional_extension_count <
         MAX_EXTENSION_COUNT);
  memcpy(requested_extensions + requested_extension_count,
         additional_extensions,
         additional_extension_count * sizeof(const char *));
  requested_extension_count += additional_extension_count;

  uint32_t enabled_layer_count = 0;
  const char *const *enabled_layers = NULL;

  VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = vulkan_debug_callback};
  if (renderer->enable_validation_layers) {
    static const char *validation_layers[] = {"VK_LAYER_KHRONOS_validation"};
    static const uint32_t validation_layer_count =
        sizeof(validation_layers) / sizeof(const char *);
    uint32_t available_layer_count;
    vkEnumerateInstanceLayerProperties(&available_layer_count, NULL);

    VkLayerProperties *available_layers =
        malloc(available_layer_count * sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(&available_layer_count,
                                       available_layers);

    bool requested_layers_found = true;
    for (uint32_t validation_layer_index = 0;
         validation_layer_index < validation_layer_count;
         validation_layer_index++) {
      bool layer_found = false;
      for (uint32_t available_layer_index = 0;
           available_layer_index < available_layer_count;
           available_layer_index++) {
        if (strcmp(validation_layers[validation_layer_index],
                   available_layers[available_layer_index].layerName) == 0) {
          layer_found = true;
          break;
        }
      }

      if (!layer_found) {
        requested_layers_found = false;
        break;
      }
    }

    free(available_layers);
    if (!requested_layers_found) {
      LOG("Not all requested layers are available.");
      goto err;
    }
    enabled_layer_count = validation_layer_count;
    enabled_layers = validation_layers;
  }

  VkInstanceCreateInfo instance_create_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &application_info,
      .enabledExtensionCount = requested_extension_count,
      .ppEnabledExtensionNames = requested_extensions,
      .enabledLayerCount = enabled_layer_count,
      .ppEnabledLayerNames = enabled_layers};
  if (renderer->enable_validation_layers) {
    instance_create_info.pNext =
        (VkDebugUtilsMessengerCreateInfoEXT *)&debug_create_info;
  }

  VkResult create_instance_result =
      vkCreateInstance(&instance_create_info, NULL, &renderer->instance);
  if (create_instance_result != VK_SUCCESS) {
    LOG("Vulkan instance creation failed, VkResult=%d", create_instance_result);
    goto err;
  }
  return true;
err:
  return false;
}

void vulkan_renderer_destroy_instance(struct vulkan_renderer *renderer) {
  vkDestroyInstance(renderer->instance, NULL);
}
VkResult vkCreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *create_info,
    const VkAllocationCallbacks *allocation_callbacks,
    VkDebugUtilsMessengerEXT *debug_messenger) {
  PFN_vkCreateDebugUtilsMessengerEXT func =
      (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
          instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != NULL) {
    return func(instance, create_info, allocation_callbacks, debug_messenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

void vkDestroyDebugUtilsMessengerEXT(VkInstance instance,
                                     VkDebugUtilsMessengerEXT debugMessenger,
                                     const VkAllocationCallbacks *pAllocator) {
  PFN_vkDestroyDebugUtilsMessengerEXT func =
      (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
          instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != NULL) {
    func(instance, debugMessenger, pAllocator);
  }
}

bool vulkan_renderer_create_debug_messenger(struct vulkan_renderer *renderer) {
  return vkCreateDebugUtilsMessengerEXT(
             renderer->instance,
             &(const VkDebugUtilsMessengerCreateInfoEXT){
                 .sType =
                     VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                 .messageSeverity =
                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                 .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                 .pfnUserCallback = vulkan_debug_callback},
             NULL, &renderer->debug_messenger) == VK_SUCCESS;
}

struct queue_family_indices {
  uint32_t graphics_family;
  uint32_t present_family;
  bool has_graphics_family;
  bool has_present_family;
};

bool queue_family_indices_is_complete(
    const struct queue_family_indices *indices) {
  return indices->has_graphics_family && indices->has_present_family;
}

#define MAX_QUEUE_FAMILY_COUNT 64
struct queue_family_indices find_queue_families(VkPhysicalDevice device,
                                                VkSurfaceKHR surface) {
  struct queue_family_indices indices = {0};

  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);

  assert(queue_family_count < MAX_QUEUE_FAMILY_COUNT);
  VkQueueFamilyProperties queue_families[MAX_QUEUE_FAMILY_COUNT];
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count,
                                           queue_families);

  for (uint32_t queue_family_index = 0; queue_family_index < queue_family_count;
       queue_family_index++) {
    VkQueueFamilyProperties *queue_family = &queue_families[queue_family_index];

    VkBool32 present_support;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, queue_family_index, surface,
                                         &present_support);

    if (queue_family->queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphics_family = queue_family_index;
      indices.has_graphics_family = true;
    }

    if (present_support) {
      indices.present_family = queue_family_index;
      indices.has_present_family = true;
    }

    if (queue_family_indices_is_complete(&indices)) {
      break;
    }
  }

  return indices;
}

bool extension_with_name_is_in_array(VkExtensionProperties *array,
                                     uint32_t length,
                                     const char *extension_name) {
  for (uint32_t index = 0; index < length; index++) {
    if (strcmp(array[index].extensionName, extension_name) == 0) {
      return true;
    }
  }

  return false;
}

bool device_supports_requested_extensions(VkPhysicalDevice device,
                                          const char **required_extensions,
                                          uint32_t required_extension_count) {
  uint32_t supported_extension_count;
  vkEnumerateDeviceExtensionProperties(device, NULL, &supported_extension_count,
                                       NULL);

  VkExtensionProperties supported_extensions[MAX_EXTENSION_COUNT] = {0};
  vkEnumerateDeviceExtensionProperties(device, NULL, &supported_extension_count,
                                       supported_extensions);
  for (uint32_t required_extension_index = 0;
       required_extension_index < required_extension_count;
       required_extension_index++) {
    if (!extension_with_name_is_in_array(
            supported_extensions, supported_extension_count,
            required_extensions[required_extension_index])) {
      return false;
    }
  }

  return true;
}

#define MAX_SWAPCHAIN_SURFACE_FORMAT_COUNT 10
#define MAX_SWAPCHAIN_SURFACE_PRESENT_MODE_COUNT 10

struct swapchain_support_details {
  VkSurfaceCapabilitiesKHR capabilities;
  VkSurfaceFormatKHR formats[MAX_SWAPCHAIN_SURFACE_FORMAT_COUNT];
  VkPresentModeKHR present_modes[MAX_SWAPCHAIN_SURFACE_PRESENT_MODE_COUNT];
  uint32_t format_count;
  uint32_t present_mode_count;
};
struct swapchain_support_details
query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface) {
  struct swapchain_support_details details;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                            &details.capabilities);

  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details.format_count,
                                       NULL);
  if (details.format_count != 0) {
    assert(details.format_count < MAX_SWAPCHAIN_SURFACE_FORMAT_COUNT);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details.format_count,
                                         details.formats);
  }

  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,
                                            &details.present_mode_count, NULL);
  if (details.present_mode_count != 0) {
    assert(details.present_mode_count <
           MAX_SWAPCHAIN_SURFACE_PRESENT_MODE_COUNT);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, surface, &details.present_mode_count, details.present_modes);
  }

  return details;
}

bool is_device_suitable(VkPhysicalDevice device, VkSurfaceKHR surface,
                        const char **required_extensions,
                        uint32_t required_extension_count) {
  struct queue_family_indices queue_family_indices =
      find_queue_families(device, surface);

  bool extensions_supported = device_supports_requested_extensions(
      device, required_extensions, required_extension_count);

  bool swapchain_adequate = false;
  if (extensions_supported) {
    struct swapchain_support_details swapchain_support_details =
        query_swapchain_support(device, surface);
    swapchain_adequate = swapchain_support_details.format_count != 0 &&
                         swapchain_support_details.present_mode_count != 0;
  }

  return queue_family_indices_is_complete(&queue_family_indices) &&
         extensions_supported && swapchain_adequate;
}

static const char *required_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
static uint32_t required_extension_count =
    sizeof(required_extensions) / sizeof(const char *);

bool vulkan_renderer_pick_physical_device(struct vulkan_renderer *renderer) {

  VkPhysicalDevice physical_device = VK_NULL_HANDLE;
  uint32_t device_count = 0;
  vkEnumeratePhysicalDevices(renderer->instance, &device_count, NULL);
  if (device_count == 0) {
    LOG("No GPU with Vulkan support found");
    goto err;
  }
  assert(device_count < MAX_DEVICE_COUNT);

  VkPhysicalDevice devices[MAX_DEVICE_COUNT];
  vkEnumeratePhysicalDevices(renderer->instance, &device_count, devices);

  for (uint32_t device_index = 0; device_index < device_count; device_index++) {
    if (is_device_suitable(devices[device_index], renderer->surface,
                           required_extensions, required_extension_count)) {
      physical_device = devices[device_index];
      break;
    }
  }

  if (physical_device == VK_NULL_HANDLE) {
    LOG("Failed to find a suitable GPU");
    goto err;
  }

  renderer->physical_device = physical_device;

  return true;
err:
  return false;
}

bool is_in_array(uint32_t *array, int length, uint32_t value) {
  for (int i = 0; i < length; i++) {
    if (array[i] == value) {
      return true;
    }
  }

  return false;
}

bool vulkan_renderer_create_logical_device(struct vulkan_renderer *renderer) {
  struct queue_family_indices indices =
      find_queue_families(renderer->physical_device, renderer->surface);
  VkDeviceQueueCreateInfo queue_create_infos[MAX_QUEUE_FAMILY_COUNT] = {0};
  int queue_create_info_count = 0;

  uint32_t unique_queue_families[MAX_QUEUE_FAMILY_COUNT] = {0};
  int unique_queue_family_count = 0;

  if (!is_in_array(unique_queue_families, unique_queue_family_count,
                   indices.graphics_family)) {
    assert(unique_queue_family_count < MAX_QUEUE_FAMILY_COUNT);
    unique_queue_families[unique_queue_family_count++] =
        indices.graphics_family;
  }
  if (!is_in_array(unique_queue_families, unique_queue_family_count,
                   indices.present_family)) {
    assert(unique_queue_family_count < MAX_QUEUE_FAMILY_COUNT);
    unique_queue_families[unique_queue_family_count++] = indices.present_family;
  }

  float queue_priority = 1.0f;
  for (int unique_queue_family_index = 0;
       unique_queue_family_index < unique_queue_family_count;
       unique_queue_family_index++) {
    queue_create_infos[queue_create_info_count++] = (VkDeviceQueueCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = unique_queue_families[unique_queue_family_index],
        .queueCount = 1,
        .pQueuePriorities = &queue_priority};
  }

  VkPhysicalDeviceFeatures device_features = {0};

  if (vkCreateDevice(renderer->physical_device,
                     &(const VkDeviceCreateInfo){
                         .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                         .pQueueCreateInfos = queue_create_infos,
                         .queueCreateInfoCount = queue_create_info_count,
                         .pEnabledFeatures = &device_features,
                         .ppEnabledExtensionNames = required_extensions,
                         .enabledExtensionCount = required_extension_count,
                         // TODO maybe add the validation layers
                         // Not required according to vulkan-tutorial, but might
                         // be good for compatibility
                     },
                     NULL, &renderer->device) != VK_SUCCESS) {
    LOG("Couldn't create logical vulkan device");
    return false;
  }

  vkGetDeviceQueue(renderer->device, indices.graphics_family, 0,
                   &renderer->graphics_queue);
  vkGetDeviceQueue(renderer->device, indices.present_family, 0,
                   &renderer->present_queue);
  LOG("graphics_queue: %p", (void *)renderer->graphics_queue);
  LOG("present_queue: %p", (void *)renderer->present_queue);

  return true;
}

VkSurfaceFormatKHR
choose_swapchain_surface_format(VkSurfaceFormatKHR *available_formats,
                                uint32_t available_format_count) {
  for (uint32_t available_format_index = 0;
       available_format_index < available_format_count;
       available_format_index++) {
    VkSurfaceFormatKHR available_format =
        available_formats[available_format_index];
    if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB &&
        available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return available_format;
    }
  }

  return available_formats[0];
}

VkPresentModeKHR
choose_swapchain_present_mode(VkPresentModeKHR *available_present_modes,
                              uint32_t available_present_mode_count) {
  for (uint32_t available_present_mode_index = 0;
       available_present_mode_index < available_present_mode_count;
       available_present_mode_index++) {
    VkPresentModeKHR present_mode =
        available_present_modes[available_present_mode_index];
    if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return present_mode;
    }
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

uint32_t clamp_uint32(uint32_t min, uint32_t max, uint32_t value) {
  return value < min ? min : value > max ? max : value;
}

VkExtent2D choose_swapchain_extent(const VkSurfaceCapabilitiesKHR *capabilities,
                                   int width, int height) {
  if (capabilities->currentExtent.width != UINT32_MAX) {
    return capabilities->currentExtent;
  } else {
    VkExtent2D actual_extent = {width, height};
    actual_extent.width =
        clamp_uint32(capabilities->minImageExtent.width,
                     capabilities->maxImageExtent.width, actual_extent.width);
    actual_extent.height =
        clamp_uint32(capabilities->minImageExtent.height,
                     capabilities->maxImageExtent.height, actual_extent.height);
    return actual_extent;
  }
}

bool vulkan_renderer_create_swapchain(struct vulkan_renderer *renderer,
                                      int window_width_px,
                                      int window_height_px) {
  struct swapchain_support_details swapchain_support =
      query_swapchain_support(renderer->physical_device, renderer->surface);

  VkSurfaceFormatKHR surface_format = choose_swapchain_surface_format(
      swapchain_support.formats, swapchain_support.format_count);
  VkPresentModeKHR present_mode = choose_swapchain_present_mode(
      swapchain_support.present_modes, swapchain_support.present_mode_count);
  VkExtent2D extent = choose_swapchain_extent(
      &swapchain_support.capabilities, window_width_px, window_height_px);
  uint32_t image_count = swapchain_support.capabilities.minImageCount + 1;
  if (swapchain_support.capabilities.maxImageCount > 0 &&
      image_count > swapchain_support.capabilities.maxImageCount) {
    image_count = swapchain_support.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR create_info = {0};
  create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  create_info.surface = renderer->surface;
  create_info.minImageCount = image_count;
  create_info.imageFormat = surface_format.format;
  create_info.imageColorSpace = surface_format.colorSpace;
  create_info.imageExtent = extent;
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  struct queue_family_indices indices =
      find_queue_families(renderer->physical_device, renderer->surface);
  uint32_t queue_family_indices[] = {indices.graphics_family,
                                     indices.present_family};

  if (indices.graphics_family != indices.present_family) {
    create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    create_info.queueFamilyIndexCount = 2;
    create_info.pQueueFamilyIndices = queue_family_indices;
  } else {
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  create_info.preTransform = swapchain_support.capabilities.currentTransform;
  create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  create_info.presentMode = present_mode;
  create_info.clipped = VK_TRUE;
  create_info.oldSwapchain = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(renderer->device, &create_info, NULL,
                           &renderer->swapchain) != VK_SUCCESS) {
    LOG("Couldn't create swapchain");
    return false;
  }

  uint32_t actual_image_count;
  vkGetSwapchainImagesKHR(renderer->device, renderer->swapchain,
                          &actual_image_count, NULL);
  assert(actual_image_count <= MAX_SWAPCHAIN_IMAGE_COUNT);
  renderer->swapchain_image_count = actual_image_count;
  vkGetSwapchainImagesKHR(renderer->device, renderer->swapchain,
                          &actual_image_count, renderer->swapchain_images);
  renderer->swapchain_image_format = surface_format.format;
  renderer->swapchain_extent = extent;
  return true;
}

bool vulkan_renderer_create_swapchain_image_views(
    struct vulkan_renderer *renderer) {
  uint32_t swapchain_image_index = 0;
  for (; swapchain_image_index < renderer->swapchain_image_count;
       swapchain_image_index++) {
    if (vkCreateImageView(
            renderer->device,
            &(const VkImageViewCreateInfo){
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = renderer->swapchain_images[swapchain_image_index],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = renderer->swapchain_image_format,
                .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                     .levelCount = 1,
                                     .layerCount = 1}},
            NULL, &renderer->swapchain_image_views[swapchain_image_index]) !=
        VK_SUCCESS) {
      goto err;
    }
  }

  return true;
err:
  // Destroying the image views that got created
  for (uint32_t image_view_index = 0; image_view_index < swapchain_image_index;
       image_view_index++) {
    vkDestroyImageView(renderer->device,
                       renderer->swapchain_image_views[image_view_index], NULL);
  }
  return false;
}

char *load_shader_from_file(const char *path, size_t *out_size) {
  FILE *file_handle = fopen(path, "rb");
  if (!file_handle) {
    goto err;
  }

  if (fseek(file_handle, 0, SEEK_END) < 0) {
    goto close_file;
  }

  long file_size = ftell(file_handle);
  if (file_size < 0) {
    goto close_file;
  }
  rewind(file_handle);
  char *shader_file_content = malloc(file_size);
  if (fread(shader_file_content, file_size, 1, file_handle) != 1) {
    goto free_shader_file_content;
  }

  if (fclose(file_handle) != 0) {
    goto err;
  }

  *out_size = file_size;
  return shader_file_content;
free_shader_file_content:
  free(shader_file_content);
close_file:
  fclose(file_handle);
err:
  return NULL;
}

VkShaderModule create_shader_module(VkDevice device, char *code,
                                    size_t code_size) {
  VkShaderModule shader_module;
  if (vkCreateShaderModule(
          device,
          &(const VkShaderModuleCreateInfo){
              .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
              .codeSize = code_size,
              .pCode = (const uint32_t *)code,
          },
          NULL, &shader_module) != VK_SUCCESS) {
    return NULL;
  }

  return shader_module;
}

bool vulkan_renderer_create_graphics_pipeline(
    struct vulkan_renderer *renderer) {
  size_t vertex_shader_code_size;
  char *vertex_shader_code = load_shader_from_file("shaders/triangle.vert.spv",
                                                   &vertex_shader_code_size);
  size_t fragment_shader_code_size;
  char *fragment_shader_code = load_shader_from_file(
      "shaders/triangle.frag.spv", &fragment_shader_code_size);
  VkShaderModule vertex_shader_module = create_shader_module(
      renderer->device, vertex_shader_code, vertex_shader_code_size);
  VkShaderModule fragment_shader_module = create_shader_module(
      renderer->device, fragment_shader_code, fragment_shader_code_size);
  free(vertex_shader_code);
  free(fragment_shader_code);

  VkPipelineShaderStageCreateInfo vertex_shader_stage_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vertex_shader_module,
      .pName = "main"};
  VkPipelineShaderStageCreateInfo fragment_shader_stage_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = fragment_shader_module,
      .pName = "main"};

  VkPipelineShaderStageCreateInfo shader_stages[] = {
      vertex_shader_stage_info, fragment_shader_stage_info};

  VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                     VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamic_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount = (sizeof(dynamic_states) / sizeof(VkDynamicState)),
      .pDynamicStates = dynamic_states};

  VkPipelineVertexInputStateCreateInfo vertex_input_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
  };

  VkPipelineInputAssemblyStateCreateInfo input_assembly = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE};

  VkViewport viewport = {.width = (float)renderer->swapchain_extent.width,
                         .height = (float)renderer->swapchain_extent.height,
                         .maxDepth = 1.0f};

  VkRect2D scissor = {.offset = {0}, .extent = renderer->swapchain_extent};

  VkPipelineViewportStateCreateInfo viewport_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1,
      .pViewports = &viewport,
      .pScissors = &scissor};

  VkPipelineRasterizationStateCreateInfo rasterizer = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .lineWidth = 1.0f,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthBiasEnable = VK_FALSE};

  VkPipelineMultisampleStateCreateInfo multisampling = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .sampleShadingEnable = VK_FALSE,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .minSampleShading = 1.0f,
  };

  VkPipelineColorBlendAttachmentState color_blend_attachment = {
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      .blendEnable = VK_FALSE,
  };

  VkPipelineColorBlendStateCreateInfo color_blending = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .attachmentCount = 1,
      .pAttachments = &color_blend_attachment};

  if (vkCreatePipelineLayout(
          renderer->device,
          &(const VkPipelineLayoutCreateInfo){
              .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
          },
          NULL, &renderer->pipeline_layout) != VK_SUCCESS) {
    goto destroy_shader_modules;
  }

  if (vkCreateGraphicsPipelines(
          renderer->device, VK_NULL_HANDLE, 1,
          &(const VkGraphicsPipelineCreateInfo){
              .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
              .stageCount = 2,
              .pStages = shader_stages,
              .pVertexInputState = &vertex_input_info,
              .pInputAssemblyState = &input_assembly,
              .pViewportState = &viewport_state,
              .pRasterizationState = &rasterizer,
              .pMultisampleState = &multisampling,
              .pColorBlendState = &color_blending,
              .pDynamicState = &dynamic_state,
              .layout = renderer->pipeline_layout,
              .renderPass = renderer->render_pass,
              .subpass = 0,

          },
          NULL, &renderer->pipeline) != VK_SUCCESS) {
    goto destroy_shader_modules;
  }

  vkDestroyShaderModule(renderer->device, vertex_shader_module, NULL);
  vkDestroyShaderModule(renderer->device, fragment_shader_module, NULL);
  return true;
destroy_shader_modules:
  vkDestroyShaderModule(renderer->device, vertex_shader_module, NULL);
  vkDestroyShaderModule(renderer->device, fragment_shader_module, NULL);
  return false;
}

bool vulkan_renderer_create_render_pass(struct vulkan_renderer *renderer) {
  VkAttachmentDescription color_attachment = {
      .format = renderer->swapchain_image_format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};

  VkAttachmentReference color_attachment_ref = {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription subpass = {.pipelineBindPoint =
                                      VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  .colorAttachmentCount = 1,
                                  .pColorAttachments = &color_attachment_ref};

  if (vkCreateRenderPass(renderer->device,
                         &(const VkRenderPassCreateInfo){
                             .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                             .attachmentCount = 1,
                             .pAttachments = &color_attachment,
                             .subpassCount = 1,
                             .pSubpasses = &subpass},
                         NULL, &renderer->render_pass) != VK_SUCCESS) {
    return false;
  }

  return true;
}

bool vulkan_renderer_create_framebuffers(struct vulkan_renderer *renderer) {
  for (uint32_t swapchain_image_view_index = 0;
       swapchain_image_view_index < renderer->swapchain_image_count;
       swapchain_image_view_index++) {
    VkImageView attachments[] = {
        renderer->swapchain_image_views[swapchain_image_view_index]};

    if (vkCreateFramebuffer(
            renderer->device,
            &(const VkFramebufferCreateInfo){
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = renderer->render_pass,
                .attachmentCount = 1,
                .pAttachments = attachments,
                .width = renderer->swapchain_extent.width,
                .height = renderer->swapchain_extent.height,
                .layers = 1},
            NULL,
            &renderer->swapchain_framebuffers[swapchain_image_view_index]) !=
        VK_SUCCESS) {
      return false;
    }
  }
  return true;
}
bool vulkan_renderer_init(struct vulkan_renderer *renderer,
                          SDL_Window *window) {
  assert(renderer);
  assert(window);
#ifdef NDEBUG
  renderer->enable_validation_layers = false;
#else
  renderer->enable_validation_layers = true;
#endif

  if (!vulkan_renderer_create_instance(renderer)) {
    goto err;
  }

  if (renderer->enable_validation_layers) {
    if (!vulkan_renderer_create_debug_messenger(renderer)) {
      LOG("Couldn't create Vulkan renderer debug messenger.");
    }
  }

  if (!SDL_Vulkan_CreateSurface(window, renderer->instance, NULL,
                                &renderer->surface)) {
    LOG("Couldn't create Vulkan rendering surface: %s", SDL_GetError());
    goto destroy_instance;
  }

  if (!vulkan_renderer_pick_physical_device(renderer)) {
    LOG("Couldn't pick the appropriate physical device.");
    goto destroy_surface;
  }

  if (!vulkan_renderer_create_logical_device(renderer)) {
    LOG("Couldn't create the logical device");
    goto destroy_surface;
  }

  int window_width_px;
  int window_height_px;
  if (!SDL_GetWindowSizeInPixels(window, &window_width_px, &window_height_px)) {
    LOG("Couldn't get window size");
    goto destroy_surface;
  }

  if (!vulkan_renderer_create_swapchain(renderer, window_width_px,
                                        window_height_px)) {
    LOG("Couldn't create swapchain");
    goto destroy_logical_device;
  }

  if (!vulkan_renderer_create_swapchain_image_views(renderer)) {
    LOG("Couldn't create swapchain image views");
    goto destroy_swapchain;
  }

  if (!vulkan_renderer_create_render_pass(renderer)) {
    LOG("Couldn't create render pass");
    goto destroy_swapchain_image_views;
  }

  if (!vulkan_renderer_create_graphics_pipeline(renderer)) {
    LOG("Couldn't create graphics pipeline");
    goto destroy_render_pass;
  }

  if (!vulkan_renderer_create_framebuffers(renderer)) {
    LOG("Couldn't create framebuffers");
    goto destroy_graphics_pipeline;
  }

  return true;

destroy_graphics_pipeline:
  vkDestroyPipeline(renderer->device, renderer->pipeline, NULL);
destroy_render_pass:
  vkDestroyPipelineLayout(renderer->device, renderer->pipeline_layout, NULL);
  vkDestroyRenderPass(renderer->device, renderer->render_pass, NULL);
destroy_swapchain_image_views:
  for (uint32_t swapchain_image_view_index = 0;
       swapchain_image_view_index < renderer->swapchain_image_count;
       swapchain_image_view_index++) {
    vkDestroyImageView(
        renderer->device,
        renderer->swapchain_image_views[swapchain_image_view_index], NULL);
  }
destroy_swapchain:
  vkDestroySwapchainKHR(renderer->device, renderer->swapchain, NULL);
destroy_logical_device:
  vkDestroyDevice(renderer->device, NULL);
destroy_surface:
  vkDestroySurfaceKHR(renderer->instance, renderer->surface, NULL);
destroy_instance:
  if (renderer->enable_validation_layers) {
    vkDestroyDebugUtilsMessengerEXT(renderer->instance,
                                    renderer->debug_messenger, NULL);
  }
  vulkan_renderer_destroy_instance(renderer);
err:
  return false;
}

void vulkan_renderer_deinit(struct vulkan_renderer *renderer) {
  for (uint32_t framebuffer_index = 0;
       framebuffer_index < renderer->swapchain_image_count;
       framebuffer_index++) {
    vkDestroyFramebuffer(renderer->device,
                         renderer->swapchain_framebuffers[framebuffer_index],
                         NULL);
  }
  vkDestroyPipeline(renderer->device, renderer->pipeline, NULL);
  vkDestroyPipelineLayout(renderer->device, renderer->pipeline_layout, NULL);
  vkDestroyRenderPass(renderer->device, renderer->render_pass, NULL);
  for (uint32_t swapchain_image_view_index = 0;
       swapchain_image_view_index < renderer->swapchain_image_count;
       swapchain_image_view_index++) {
    vkDestroyImageView(
        renderer->device,
        renderer->swapchain_image_views[swapchain_image_view_index], NULL);
  }
  vkDestroySwapchainKHR(renderer->device, renderer->swapchain, NULL);
  vkDestroyDevice(renderer->device, NULL);
  vkDestroySurfaceKHR(renderer->instance, renderer->surface, NULL);
  if (renderer->enable_validation_layers) {
    vkDestroyDebugUtilsMessengerEXT(renderer->instance,
                                    renderer->debug_messenger, NULL);
  }
  vulkan_renderer_destroy_instance(renderer);
}

int main(void) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    LOG("Couldn't initialize SDL: %s", SDL_GetError());
    goto err;
  }

  SDL_Window *window =
      SDL_CreateWindow("vkguide", 1280, 720, SDL_WINDOW_VULKAN);
  if (!window) {
    LOG("Couldn't create window: %s", SDL_GetError());
    goto quit_sdl;
  }

  struct vulkan_renderer renderer;
  if (!vulkan_renderer_init(&renderer, window)) {
    LOG("Couldn't init vulkan renderer");
    goto destroy_window;
  }

  while (true) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      bool quit = event.type == SDL_EVENT_QUIT;
      bool escape_pressed =
          event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE;

      if (quit || escape_pressed) {
        goto out_main_loop;
      }
    }

    // Render things
  }
out_main_loop:

  vulkan_renderer_deinit(&renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;

destroy_window:
  SDL_DestroyWindow(window);
quit_sdl:
  SDL_Quit();
err:
  return 1;
}

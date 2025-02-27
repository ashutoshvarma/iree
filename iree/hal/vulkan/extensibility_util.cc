// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "iree/hal/vulkan/extensibility_util.h"

#include "iree/hal/vulkan/status_util.h"

// Returns true if |layers| contains a layer matching |layer_name|.
static bool iree_hal_vulkan_layer_list_contains(uint32_t layer_count,
                                                const VkLayerProperties* layers,
                                                const char* layer_name) {
  for (uint32_t i = 0; i < layer_count; ++i) {
    if (strcmp(layer_name, layers[i].layerName) == 0) {
      return true;
    }
  }
  return false;
}

static iree_status_t iree_hal_vulkan_match_available_layers(
    iree_host_size_t available_layers_count,
    const VkLayerProperties* available_layers,
    const iree_hal_vulkan_string_list_t* required_layers,
    const iree_hal_vulkan_string_list_t* optional_layers,
    iree_hal_vulkan_string_list_t* out_enabled_layers) {
  memset(out_enabled_layers->values, 0,
         (required_layers->count + optional_layers->count) *
             sizeof(out_enabled_layers->values[0]));

  for (iree_host_size_t i = 0; i < required_layers->count; ++i) {
    const char* layer_name = required_layers->values[i];
    if (!iree_hal_vulkan_layer_list_contains(available_layers_count,
                                             available_layers, layer_name)) {
      return iree_make_status(IREE_STATUS_UNAVAILABLE,
                              "required layer %s not available", layer_name);
    }
    out_enabled_layers->values[out_enabled_layers->count++] = layer_name;
  }

  for (iree_host_size_t i = 0; i < optional_layers->count; ++i) {
    const char* layer_name = optional_layers->values[i];
    if (iree_hal_vulkan_layer_list_contains(available_layers_count,
                                            available_layers, layer_name)) {
      out_enabled_layers->values[out_enabled_layers->count++] = layer_name;
    }
  }

  return iree_ok_status();
}

iree_status_t iree_hal_vulkan_match_available_instance_layers(
    const iree::hal::vulkan::DynamicSymbols* syms,
    const iree_hal_vulkan_string_list_t* required_layers,
    const iree_hal_vulkan_string_list_t* optional_layers, iree::Arena* arena,
    iree_hal_vulkan_string_list_t* out_enabled_layers) {
  uint32_t layer_property_count = 0;
  VK_RETURN_IF_ERROR(
      syms->vkEnumerateInstanceLayerProperties(&layer_property_count, NULL),
      "vkEnumerateInstanceLayerProperties");
  VkLayerProperties* layer_properties =
      (VkLayerProperties*)arena->AllocateBytes(layer_property_count *
                                               sizeof(VkLayerProperties));
  VK_RETURN_IF_ERROR(syms->vkEnumerateInstanceLayerProperties(
                         &layer_property_count, layer_properties),
                     "vkEnumerateInstanceLayerProperties");
  out_enabled_layers->count = 0;
  out_enabled_layers->values = (const char**)arena->AllocateBytes(
      (required_layers->count + optional_layers->count) *
      sizeof(out_enabled_layers->values[0]));
  return iree_hal_vulkan_match_available_layers(
      layer_property_count, layer_properties, required_layers, optional_layers,
      out_enabled_layers);
}

// Returns true if |extensions| contains a layer matching |extension_name|.
static bool iree_hal_vulkan_extension_list_contains(
    uint32_t extension_count, const VkExtensionProperties* extensions,
    const char* extension_name) {
  for (uint32_t i = 0; i < extension_count; ++i) {
    if (strcmp(extension_name, extensions[i].extensionName) == 0) {
      return true;
    }
  }
  return false;
}

static iree_status_t iree_hal_vulkan_match_available_extensions(
    iree_host_size_t available_extension_count,
    const VkExtensionProperties* available_extensions,
    const iree_hal_vulkan_string_list_t* required_extensions,
    const iree_hal_vulkan_string_list_t* optional_extensions,
    iree_hal_vulkan_string_list_t* out_enabled_extensions) {
  memset(out_enabled_extensions->values, 0,
         (required_extensions->count + optional_extensions->count) *
             sizeof(out_enabled_extensions->values[0]));

  for (iree_host_size_t i = 0; i < required_extensions->count; ++i) {
    const char* extension_name = required_extensions->values[i];
    if (!iree_hal_vulkan_extension_list_contains(
            available_extension_count, available_extensions, extension_name)) {
      return iree_make_status(IREE_STATUS_UNAVAILABLE,
                              "required extension %s not available",
                              extension_name);
    }
    out_enabled_extensions->values[out_enabled_extensions->count++] =
        extension_name;
  }

  for (iree_host_size_t i = 0; i < optional_extensions->count; ++i) {
    const char* extension_name = optional_extensions->values[i];
    if (iree_hal_vulkan_extension_list_contains(
            available_extension_count, available_extensions, extension_name)) {
      out_enabled_extensions->values[out_enabled_extensions->count++] =
          extension_name;
    }
  }

  return iree_ok_status();
}

iree_status_t iree_hal_vulkan_match_available_instance_extensions(
    const iree::hal::vulkan::DynamicSymbols* syms,
    const iree_hal_vulkan_string_list_t* required_extensions,
    const iree_hal_vulkan_string_list_t* optional_extensions,
    iree::Arena* arena, iree_hal_vulkan_string_list_t* out_enabled_extensions) {
  uint32_t extension_property_count = 0;
  VK_RETURN_IF_ERROR(syms->vkEnumerateInstanceExtensionProperties(
                         NULL, &extension_property_count, NULL),
                     "vkEnumerateInstanceExtensionProperties");
  VkExtensionProperties* extension_properties =
      (VkExtensionProperties*)arena->AllocateBytes(
          extension_property_count * sizeof(VkExtensionProperties));
  VK_RETURN_IF_ERROR(syms->vkEnumerateInstanceExtensionProperties(
                         NULL, &extension_property_count, extension_properties),
                     "vkEnumerateInstanceExtensionProperties");
  out_enabled_extensions->count = 0;
  out_enabled_extensions->values = (const char**)arena->AllocateBytes(
      (required_extensions->count + optional_extensions->count) *
      sizeof(out_enabled_extensions->values[0]));
  return iree_hal_vulkan_match_available_extensions(
      extension_property_count, extension_properties, required_extensions,
      optional_extensions, out_enabled_extensions);
}

iree_status_t iree_hal_vulkan_match_available_device_extensions(
    const iree::hal::vulkan::DynamicSymbols* syms,
    VkPhysicalDevice physical_device,
    const iree_hal_vulkan_string_list_t* required_extensions,
    const iree_hal_vulkan_string_list_t* optional_extensions,
    iree::Arena* arena, iree_hal_vulkan_string_list_t* out_enabled_extensions) {
  uint32_t extension_property_count = 0;
  VK_RETURN_IF_ERROR(
      syms->vkEnumerateDeviceExtensionProperties(
          physical_device, NULL, &extension_property_count, NULL),
      "vkEnumerateDeviceExtensionProperties");
  VkExtensionProperties* extension_properties =
      (VkExtensionProperties*)arena->AllocateBytes(
          extension_property_count * sizeof(VkExtensionProperties));
  VK_RETURN_IF_ERROR(syms->vkEnumerateDeviceExtensionProperties(
                         physical_device, NULL, &extension_property_count,
                         extension_properties),
                     "vkEnumerateDeviceExtensionProperties");
  out_enabled_extensions->count = 0;
  out_enabled_extensions->values = (const char**)arena->AllocateBytes(
      (required_extensions->count + optional_extensions->count) *
      sizeof(out_enabled_extensions->values[0]));
  return iree_hal_vulkan_match_available_extensions(
      extension_property_count, extension_properties, required_extensions,
      optional_extensions, out_enabled_extensions);
}

iree_hal_vulkan_instance_extensions_t
iree_hal_vulkan_populate_enabled_instance_extensions(
    const iree_hal_vulkan_string_list_t* enabled_extensions) {
  iree_hal_vulkan_instance_extensions_t extensions;
  memset(&extensions, 0, sizeof(extensions));
  for (iree_host_size_t i = 0; i < enabled_extensions->count; ++i) {
    const char* extension_name = enabled_extensions->values[i];
    if (strcmp(extension_name, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
      extensions.debug_utils = true;
    }
  }
  return extensions;
}

iree_hal_vulkan_device_extensions_t
iree_hal_vulkan_populate_enabled_device_extensions(
    const iree_hal_vulkan_string_list_t* enabled_extensions) {
  iree_hal_vulkan_device_extensions_t extensions;
  memset(&extensions, 0, sizeof(extensions));
  for (iree_host_size_t i = 0; i < enabled_extensions->count; ++i) {
    const char* extension_name = enabled_extensions->values[i];
    if (strcmp(extension_name, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME) == 0) {
      extensions.push_descriptors = true;
    } else if (strcmp(extension_name,
                      VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME) == 0) {
      extensions.timeline_semaphore = true;
    }
  }
  return extensions;
}

iree_hal_vulkan_device_extensions_t
iree_hal_vulkan_infer_enabled_device_extensions(
    const iree::hal::vulkan::DynamicSymbols* device_syms) {
  iree_hal_vulkan_device_extensions_t extensions;
  memset(&extensions, 0, sizeof(extensions));
  if (device_syms->vkCmdPushDescriptorSetKHR) {
    extensions.push_descriptors = true;
  }
  if (device_syms->vkSignalSemaphore || device_syms->vkSignalSemaphoreKHR) {
    extensions.timeline_semaphore = true;
  }
  return extensions;
}

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

#ifndef IREE_HAL_VULKAN_EXTENSIBILITY_UTIL_H_
#define IREE_HAL_VULKAN_EXTENSIBILITY_UTIL_H_

#include "iree/hal/vulkan/api.h"
#include "iree/hal/vulkan/dynamic_symbols.h"
#include "iree/hal/vulkan/util/arena.h"

// A list of NUL-terminated strings (so they can be passed directly to Vulkan).
typedef struct {
  iree_host_size_t count;
  const char** values;
} iree_hal_vulkan_string_list_t;

// Populates |out_enabled_layers| with all layers that are both available in the
// implementation and |required_layers| and |optional_layers| lists.
// |out_enabled_layers| must have capacity at least the sum of
// |required_layers|.count and |optional_layer|.count.
// Returns failure if any |required_layers| are unavailable.
iree_status_t iree_hal_vulkan_match_available_instance_layers(
    const iree::hal::vulkan::DynamicSymbols* syms,
    const iree_hal_vulkan_string_list_t* required_layers,
    const iree_hal_vulkan_string_list_t* optional_layers, iree::Arena* arena,
    iree_hal_vulkan_string_list_t* out_enabled_layers);

// Populates |out_enabled_extensions| with all extensions that are both
// available in the implementation and |required_extensions| and
// |optional_extensions| lists. |out_enabled_extensions| must have capacity at
// least the sum of |required_extensions|.count and |optional_extensions|.count.
// Returns failure if any |required_extensions| are unavailable.
iree_status_t iree_hal_vulkan_match_available_instance_extensions(
    const iree::hal::vulkan::DynamicSymbols* syms,
    const iree_hal_vulkan_string_list_t* required_extensions,
    const iree_hal_vulkan_string_list_t* optional_extensions,
    iree::Arena* arena, iree_hal_vulkan_string_list_t* out_enabled_extensions);

// Populates |out_enabled_extensions| with all extensions that are both
// available in the implementation and |required_extensions| and
// |optional_extensions| lists. |out_enabled_extensions| must have capacity at
// least the sum of |required_extensions|.count and |optional_extensions|.count.
// Returns failure if any |required_extensions| are unavailable.
iree_status_t iree_hal_vulkan_match_available_device_extensions(
    const iree::hal::vulkan::DynamicSymbols* syms,
    VkPhysicalDevice physical_device,
    const iree_hal_vulkan_string_list_t* required_extensions,
    const iree_hal_vulkan_string_list_t* optional_extensions,
    iree::Arena* arena, iree_hal_vulkan_string_list_t* out_enabled_extensions);

// Bits for enabled instance extensions.
// We must use this to query support instead of just detecting symbol names as
// ICDs will resolve the functions sometimes even if they don't support the
// extension (or we didn't ask for it to be enabled).
typedef struct {
  // VK_EXT_debug_utils is enabled and a debug messenger is registered.
  // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap44.html#VK_EXT_debug_utils
  bool debug_utils : 1;
} iree_hal_vulkan_instance_extensions_t;

// Returns a bitfield with all of the provided extension names.
iree_hal_vulkan_instance_extensions_t
iree_hal_vulkan_populate_enabled_instance_extensions(
    const iree_hal_vulkan_string_list_t* enabled_extension);

// Bits for enabled device extensions.
// We must use this to query support instead of just detecting symbol names as
// ICDs will resolve the functions sometimes even if they don't support the
// extension (or we didn't ask for it to be enabled).
typedef struct {
  // VK_KHR_push_descriptor is enabled and vkCmdPushDescriptorSetKHR is valid.
  bool push_descriptors : 1;
  // VK_KHR_timeline_semaphore is enabled.
  bool timeline_semaphore : 1;
} iree_hal_vulkan_device_extensions_t;

// Returns a bitfield with all of the provided extension names.
iree_hal_vulkan_device_extensions_t
iree_hal_vulkan_populate_enabled_device_extensions(
    const iree_hal_vulkan_string_list_t* enabled_extension);

// Returns a bitfield with the extensions that are (likely) available on the
// device symbols. This is less reliable than setting the bits directly when
// the known set of extensions is available.
iree_hal_vulkan_device_extensions_t
iree_hal_vulkan_infer_enabled_device_extensions(
    const iree::hal::vulkan::DynamicSymbols* device_syms);

#endif  // IREE_HAL_VULKAN_EXTENSIBILITY_UTIL_H_

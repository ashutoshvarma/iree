// Copyright 2020 Google LLC
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

#include "iree/hal/vulkan/native_descriptor_set.h"

#include "iree/base/tracing.h"

using namespace iree::hal::vulkan;

typedef struct {
  iree_hal_resource_t resource;
  VkDeviceHandle* logical_device;
  VkDescriptorSet handle;
} iree_hal_vulkan_native_descriptor_set_t;

extern const iree_hal_descriptor_set_vtable_t
    iree_hal_vulkan_native_descriptor_set_vtable;

static iree_hal_vulkan_native_descriptor_set_t*
iree_hal_vulkan_native_descriptor_set_cast(
    iree_hal_descriptor_set_t* base_value) {
  IREE_HAL_ASSERT_TYPE(base_value,
                       &iree_hal_vulkan_native_descriptor_set_vtable);
  return (iree_hal_vulkan_native_descriptor_set_t*)base_value;
}

iree_status_t iree_hal_vulkan_native_descriptor_set_create(
    iree::hal::vulkan::VkDeviceHandle* logical_device, VkDescriptorSet handle,
    iree_hal_descriptor_set_t** out_descriptor_set) {
  IREE_ASSERT_ARGUMENT(logical_device);
  IREE_ASSERT_ARGUMENT(handle);
  IREE_ASSERT_ARGUMENT(out_descriptor_set);
  *out_descriptor_set = NULL;
  IREE_TRACE_ZONE_BEGIN(z0);

  iree_hal_vulkan_native_descriptor_set_t* descriptor_set = NULL;
  iree_status_t status =
      iree_allocator_malloc(logical_device->host_allocator(),
                            sizeof(*descriptor_set), (void**)&descriptor_set);
  if (iree_status_is_ok(status)) {
    iree_hal_resource_initialize(&iree_hal_vulkan_native_descriptor_set_vtable,
                                 &descriptor_set->resource);
    descriptor_set->logical_device = logical_device;
    descriptor_set->handle = handle;
    *out_descriptor_set = (iree_hal_descriptor_set_t*)descriptor_set;
  }

  IREE_TRACE_ZONE_END(z0);
  return status;
}

static void iree_hal_vulkan_native_descriptor_set_destroy(
    iree_hal_descriptor_set_t* base_descriptor_set) {
  iree_hal_vulkan_native_descriptor_set_t* descriptor_set =
      iree_hal_vulkan_native_descriptor_set_cast(base_descriptor_set);
  iree_allocator_t host_allocator =
      descriptor_set->logical_device->host_allocator();
  IREE_TRACE_ZONE_BEGIN(z0);

  // TODO(benvanik): return to pool. For now we rely on the descriptor cache to
  // reset entire pools at once via via vkResetDescriptorPool so we don't need
  // to do anything here (the VkDescriptorSet handle will just be invalidated).
  // In the future if we want to have generational collection/defragmentation
  // of the descriptor cache we'll want to allow both pooled and unpooled
  // descriptors and clean them up here appropriately.

  iree_allocator_free(host_allocator, descriptor_set);

  IREE_TRACE_ZONE_END(z0);
}

VkDescriptorSet iree_hal_vulkan_native_descriptor_set_handle(
    iree_hal_descriptor_set_t* base_descriptor_set) {
  iree_hal_vulkan_native_descriptor_set_t* descriptor_set =
      iree_hal_vulkan_native_descriptor_set_cast(base_descriptor_set);
  return descriptor_set->handle;
}

const iree_hal_descriptor_set_vtable_t
    iree_hal_vulkan_native_descriptor_set_vtable = {
        /*.destroy=*/iree_hal_vulkan_native_descriptor_set_destroy,
};

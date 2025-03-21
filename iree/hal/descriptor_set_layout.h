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

#ifndef IREE_HAL_DESCRIPTOR_SET_LAYOUT_H_
#define IREE_HAL_DESCRIPTOR_SET_LAYOUT_H_

#include <stdbool.h>
#include <stdint.h>

#include "iree/base/api.h"
#include "iree/hal/buffer.h"
#include "iree/hal/resource.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef struct iree_hal_device_s iree_hal_device_t;

//===----------------------------------------------------------------------===//
// Types and Enums
//===----------------------------------------------------------------------===//

// Specifies the type of a descriptor in a descriptor set.
enum iree_hal_descriptor_type_e {
  IREE_HAL_DESCRIPTOR_TYPE_UNIFORM_BUFFER = 6u,
  IREE_HAL_DESCRIPTOR_TYPE_STORAGE_BUFFER = 7u,
  IREE_HAL_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC = 8u,
  IREE_HAL_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC = 9u,
};
typedef uint32_t iree_hal_descriptor_type_t;

// Specifies the usage type of the descriptor set.
enum iree_hal_descriptor_set_layout_usage_type_e {
  // Descriptor set will be initialized once and never changed.
  IREE_HAL_DESCRIPTOR_SET_LAYOUT_USAGE_TYPE_IMMUTABLE = 0u,
  // Descriptor set is never created and instead used with push descriptors.
  IREE_HAL_DESCRIPTOR_SET_LAYOUT_USAGE_TYPE_PUSH_ONLY = 1u,
};
typedef uint32_t iree_hal_descriptor_set_layout_usage_type_t;

// Specifies a descriptor set layout binding.
//
// Maps to VkDescriptorSetLayoutBinding.
typedef struct {
  // The binding number of this entry and corresponds to a resource of the
  // same binding number in the executable interface.
  uint32_t binding;
  // Specifies which type of resource descriptors are used for this binding.
  iree_hal_descriptor_type_t type;
  // Specifies the memory access performed by the executables.
  iree_hal_memory_access_t access;
} iree_hal_descriptor_set_layout_binding_t;

//===----------------------------------------------------------------------===//
// iree_hal_descriptor_set_layout_t
//===----------------------------------------------------------------------===//

// Opaque handle to a descriptor set layout object.
// A "descriptor" is effectively a bound memory range and each dispatch can use
// one or more "descriptor sets" to access their I/O memory. A "descriptor set
// layout" defines the types and usage semantics of the descriptors that make up
// one set. Implementations can use this to verify program correctness and
// accelerate reservation/allocatation/computation of descriptor-related
// operations.
//
// Maps to VkDescriptorSetLayout:
// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkDescriptorSetLayout.html
typedef struct iree_hal_descriptor_set_layout_s
    iree_hal_descriptor_set_layout_t;

// Creates a descriptor set layout with the given bindings.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_descriptor_set_layout_create(
    iree_hal_device_t* device,
    iree_hal_descriptor_set_layout_usage_type_t usage_type,
    iree_host_size_t binding_count,
    const iree_hal_descriptor_set_layout_binding_t* bindings,
    iree_hal_descriptor_set_layout_t** out_descriptor_set_layout);

// Retains the given |descriptor_set_layout| for the caller.
IREE_API_EXPORT void IREE_API_CALL iree_hal_descriptor_set_layout_retain(
    iree_hal_descriptor_set_layout_t* descriptor_set_layout);

// Releases the given |descriptor_set_layout| from the caller.
IREE_API_EXPORT void IREE_API_CALL iree_hal_descriptor_set_layout_release(
    iree_hal_descriptor_set_layout_t* descriptor_set_layout);

//===----------------------------------------------------------------------===//
// iree_hal_descriptor_set_layout_t implementation details
//===----------------------------------------------------------------------===//

typedef struct {
  // << HAL C porting in progress >>
  IREE_API_UNSTABLE

  void(IREE_API_PTR* destroy)(
      iree_hal_descriptor_set_layout_t* descriptor_set_layout);
} iree_hal_descriptor_set_layout_vtable_t;

IREE_API_EXPORT void IREE_API_CALL iree_hal_descriptor_set_layout_destroy(
    iree_hal_descriptor_set_layout_t* descriptor_set_layout);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // IREE_HAL_DESCRIPTOR_SET_LAYOUT_H_

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

#ifndef IREE_HAL_DRIVER_H_
#define IREE_HAL_DRIVER_H_

#include <stdbool.h>
#include <stdint.h>

#include "iree/base/api.h"
#include "iree/hal/device.h"
#include "iree/hal/resource.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

//===----------------------------------------------------------------------===//
// Types and Enums
//===----------------------------------------------------------------------===//

// An opaque factory-specific handle to identify different drivers.
typedef uint64_t iree_hal_driver_id_t;

#define IREE_HAL_DRIVER_ID_INVALID 0ull

// Describes a driver providing device enumeration and creation.
// The lifetime of memory referenced by this structure (such as strings) is
// dependent on where it originated.
//
// * When using iree_hal_driver_registry_enumerate the driver info is copied
//   into memory owned by the caller.
// * When queried from a live driver with iree_hal_driver_info the memory is
//   only guaranteed to live for as long as the driver is.
// * When enumerating via factories the information may be valid only while the
//   driver registry lock is held.
typedef struct {
  IREE_API_UNSTABLE

  // Opaque handle used by factories. Unique across all factories.
  iree_hal_driver_id_t driver_id;

  // Canonical name of the driver as used in command lines, documentation, etc.
  // Examples: 'metal', 'vulkan'
  iree_string_view_t driver_name;

  // Full human-readable name of the driver for display.
  // Examples: 'Vulkan 1.2 (NVIDIA)'.
  iree_string_view_t full_name;

  // TODO(benvanik): version information; useful if wanting to expose multiple
  // versions that may have completely different implementations (like vulkan
  // 1.0, 1.1, and 1.2) but allow a nice sort/selection process.
  // TODO(benvanik): triple, feature flags, etc.
} iree_hal_driver_info_t;

//===----------------------------------------------------------------------===//
// iree_hal_driver_t
//===----------------------------------------------------------------------===//

typedef struct iree_hal_driver_s iree_hal_driver_t;

// Retains the given |driver| for the caller.
IREE_API_EXPORT void IREE_API_CALL
iree_hal_driver_retain(iree_hal_driver_t* driver);

// Releases the given |driver| from the caller.
IREE_API_EXPORT void IREE_API_CALL
iree_hal_driver_release(iree_hal_driver_t* driver);

// Queries available devices and returns them as a list.
// The provided |allocator| will be used to allocate the returned list and after
// the caller is done with it |out_device_infos| must be freed with that same
// allocator by the caller.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_driver_query_available_devices(
    iree_hal_driver_t* driver, iree_allocator_t allocator,
    iree_hal_device_info_t** out_device_infos,
    iree_host_size_t* out_device_info_count);

// Creates a device as queried with iree_hal_driver_query_available_devices.
IREE_API_EXPORT iree_status_t IREE_API_CALL iree_hal_driver_create_device(
    iree_hal_driver_t* driver, iree_hal_device_id_t device_id,
    iree_allocator_t allocator, iree_hal_device_t** out_device);

// Creates the driver-defined "default" device. This may simply be the first
// device enumerated.
IREE_API_EXPORT iree_status_t IREE_API_CALL
iree_hal_driver_create_default_device(iree_hal_driver_t* driver,
                                      iree_allocator_t allocator,
                                      iree_hal_device_t** out_device);

//===----------------------------------------------------------------------===//
// iree_hal_driver_t implementation details
//===----------------------------------------------------------------------===//

typedef struct {
  // << HAL C porting in progress >>
  IREE_API_UNSTABLE

  void(IREE_API_PTR* destroy)(iree_hal_driver_t* driver);

  iree_status_t(IREE_API_PTR* query_available_devices)(
      iree_hal_driver_t* driver, iree_allocator_t allocator,
      iree_hal_device_info_t** out_device_infos,
      iree_host_size_t* out_device_info_count);

  iree_status_t(IREE_API_PTR* create_device)(iree_hal_driver_t* driver,
                                             iree_hal_device_id_t device_id,
                                             iree_allocator_t allocator,
                                             iree_hal_device_t** out_device);
} iree_hal_driver_vtable_t;

IREE_API_EXPORT void IREE_API_CALL
iree_hal_driver_destroy(iree_hal_driver_t* driver);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // IREE_HAL_DRIVER_H_

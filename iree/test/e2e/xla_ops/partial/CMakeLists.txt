# Copyright 2021 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

iree_add_all_subdirs()

file(GLOB _GLOB_X_MLIR LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS *.mlir)
iree_check_single_backend_test_suite(
  NAME
    check_vmla_vmla
  SRCS
    "${_GLOB_X_MLIR}"
  TARGET_BACKEND
    "vmla"
  DRIVER
    "vmla"
)

iree_check_single_backend_test_suite(
  NAME
    check_vulkan-spirv_vulkan
  TARGET_BACKEND
    "vulkan-spirv"
  DRIVER
    "vulkan"
)

iree_check_single_backend_test_suite(
  NAME
    check_dylib-llvm-aot_dylib
  TARGET_BACKEND
    "dylib-llvm-aot"
  DRIVER
    "dylib"
)

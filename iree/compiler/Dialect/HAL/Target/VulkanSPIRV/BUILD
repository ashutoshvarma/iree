# Copyright 2019 Google LLC
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

load("//iree:build_defs.oss.bzl", "iree_cmake_extra_content")

package(
    default_visibility = ["//visibility:public"],
    features = ["layering_check"],
    licenses = ["notice"],  # Apache 2.0
)

iree_cmake_extra_content(
    content = """
if(NOT "${IREE_TARGET_BACKEND_VULKAN-SPIRV}")
  return()
endif()
""",
)

cc_library(
    name = "VulkanSPIRV",
    srcs = [
        "VulkanSPIRVTarget.cpp",
    ],
    hdrs = [
        "VulkanSPIRVTarget.h",
    ],
    deps = [
        "//iree/base:flatcc",
        "//iree/compiler/Conversion/Common",
        "//iree/compiler/Conversion/LinalgToSPIRV",
        "//iree/compiler/Conversion/LinalgToSPIRV:CodeGenOptionUtils",
        "//iree/compiler/Dialect/Flow/IR",
        "//iree/compiler/Dialect/HAL/Target",
        "//iree/compiler/Dialect/HAL/Target/SPIRVCommon",
        "//iree/compiler/Dialect/Vulkan/IR",
        "//iree/compiler/Dialect/Vulkan/Utils",
        "//iree/compiler/Utils",
        "//iree/schemas:spirv_executable_def_c_fbs",
        "@llvm-project//llvm:Support",
        "@llvm-project//mlir:Affine",
        "@llvm-project//mlir:GPUDialect",
        "@llvm-project//mlir:IR",
        "@llvm-project//mlir:LinalgOps",
        "@llvm-project//mlir:Parser",
        "@llvm-project//mlir:SPIRVDialect",
        "@llvm-project//mlir:SPIRVSerialization",
        "@llvm-project//mlir:Support",
        "@llvm-project//mlir:VectorOps",
    ],
)

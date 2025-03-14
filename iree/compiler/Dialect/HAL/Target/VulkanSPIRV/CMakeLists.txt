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

if(NOT "${IREE_TARGET_BACKEND_VULKAN-SPIRV}")
  return()
endif()

iree_add_all_subdirs()

iree_cc_library(
  NAME
    VulkanSPIRV
  HDRS
    "VulkanSPIRVTarget.h"
  SRCS
    "VulkanSPIRVTarget.cpp"
  DEPS
    LLVMSupport
    MLIRAffine
    MLIRGPU
    MLIRIR
    MLIRLinalg
    MLIRParser
    MLIRSPIRV
    MLIRSPIRVSerialization
    MLIRSupport
    MLIRVector
    iree::base::flatcc
    iree::compiler::Conversion::Common
    iree::compiler::Conversion::LinalgToSPIRV
    iree::compiler::Conversion::LinalgToSPIRV::CodeGenOptionUtils
    iree::compiler::Dialect::Flow::IR
    iree::compiler::Dialect::HAL::Target
    iree::compiler::Dialect::HAL::Target::SPIRVCommon
    iree::compiler::Dialect::Vulkan::IR
    iree::compiler::Dialect::Vulkan::Utils
    iree::compiler::Utils
    iree::schemas::spirv_executable_def_c_fbs
  PUBLIC
)

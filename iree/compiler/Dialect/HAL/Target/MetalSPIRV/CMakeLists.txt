# Copyright 2020 Google LLC
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

if(NOT "${IREE_TARGET_BACKEND_METAL-SPIRV}")
  return()
endif()

iree_add_all_subdirs()

iree_cc_library(
  NAME
    MetalSPIRV
  HDRS
    "MetalSPIRVTarget.h"
  SRCS
    "MetalSPIRVTarget.cpp"
  DEPS
    ::SPIRVToMSL
    LLVMSupport
    MLIRIR
    MLIRSPIRV
    MLIRSPIRVSerialization
    MLIRSupport
    iree::base::flatcc
    iree::compiler::Dialect::Flow::IR
    iree::compiler::Dialect::HAL::Target::SPIRVCommon
    iree::compiler::Utils
    iree::schemas::metal_executable_def_c_fbs
  PUBLIC
)

iree_cc_library(
  NAME
    SPIRVToMSL
  HDRS
    "SPIRVToMSL.h"
  SRCS
    "SPIRVToMSL.cpp"
  DEPS
    LLVMSupport
    MLIRSupport
    spirv-cross-msl
  PUBLIC
)

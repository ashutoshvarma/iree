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

if(NOT "${IREE_TARGET_BACKEND_VMLA}")
  return()
endif()

iree_add_all_subdirs()

iree_cc_library(
  NAME
    VMLA
  HDRS
    "VMLATarget.h"
  SRCS
    "VMLATarget.cpp"
  DEPS
    LLVMSupport
    MLIRIR
    MLIRPass
    MLIRSupport
    iree::base::flatcc
    iree::compiler::Dialect::Flow::IR
    iree::compiler::Dialect::HAL::Target
    iree::compiler::Dialect::VM::Conversion
    iree::compiler::Dialect::VM::IR
    iree::compiler::Dialect::VM::Target::Bytecode
    iree::compiler::Dialect::VM::Transforms
    iree::compiler::Dialect::VMLA::IR::VMLADialect
    iree::compiler::Dialect::VMLA::Transforms
    iree::compiler::Utils
    iree::schemas::vmla_executable_def_c_fbs
  PUBLIC
)

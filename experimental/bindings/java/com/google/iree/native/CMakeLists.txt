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

iree_cc_library(
  NAME
    cc_wrappers
  SRCS
    "context_wrapper.cc"
    "function_wrapper.cc"
    "instance_wrapper.cc"
    "module_wrapper.cc"
  HDRS
    "context_wrapper.h"
    "function_wrapper.h"
    "instance_wrapper.h"
    "module_wrapper.h"
  DEPS
    iree::base::api
    iree::base::flags
    iree::base::logging
    iree::base::status
    iree::hal::api
    iree::hal::drivers
    iree::modules::hal
    iree::modules::strings::strings_module
    iree::modules::tensorlist::native_module
    iree::vm
    iree::vm::cc
    iree::vm::bytecode_module
)

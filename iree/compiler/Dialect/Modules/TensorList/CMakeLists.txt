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

iree_add_all_subdirs()

file(GLOB _GLOB_X_TD LIST_DIRECTORIES false RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS *.td)
iree_cc_embed_data(
  NAME
    tensorlist_imports
  SRCS
    "tensorlist.imports.mlir"
  CC_FILE_OUTPUT
    "tensorlist.imports.cc"
  H_FILE_OUTPUT
    "tensorlist.imports.h"
  CPP_NAMESPACE
    "mlir::iree_compiler::IREE::TensorList"
  FLATTEN
  PUBLIC
)

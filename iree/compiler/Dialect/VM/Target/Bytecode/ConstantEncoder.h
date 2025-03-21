// Copyright 2019 Google LLC
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

#ifndef IREE_COMPILER_DIALECT_VM_TARGET_BYTECODE_CONSTANTENCODER_H_
#define IREE_COMPILER_DIALECT_VM_TARGET_BYTECODE_CONSTANTENCODER_H_

#include "iree/compiler/Utils/FlatbufferUtils.h"
#include "iree/schemas/bytecode_module_def_builder.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/Location.h"

namespace mlir {
namespace iree_compiler {
namespace IREE {
namespace VM {

// Serializes a constant attribute to the FlatBuffer as a binary blob.
flatbuffers_uint8_vec_ref_t serializeConstant(Location loc,
                                              ElementsAttr elementsAttr,
                                              FlatbufferBuilder &fbb);

}  // namespace VM
}  // namespace IREE
}  // namespace iree_compiler
}  // namespace mlir

#endif  // IREE_COMPILER_DIALECT_VM_TARGET_BYTECODE_CONSTANTENCODER_H_

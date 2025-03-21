// Copyright 2021 Google LLC
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

#ifndef IREE_COMPILER_DIALECT_FLOW_TRANSFORMS_DESTRUCTIVEUPDATEUTILS_H_
#define IREE_COMPILER_DIALECT_FLOW_TRANSFORMS_DESTRUCTIVEUPDATEUTILS_H_

#include "iree/compiler/Dialect/Flow/IR/FlowOps.h"
#include "llvm/ADT/StringMap.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Support/LLVM.h"

namespace mlir {
namespace iree_compiler {
namespace IREE {
namespace Flow {

// Traverse `funcOp` and rewrite specific SubTensor / SubTensorInsert ops that
// match a "destructive tensor update" pattern, by an inplace update.
// This serves as a step in jumping the abstraction gap between transformed
// "linalg on tensors" IR (in which the whole tensor is updated) and dispatch
// regions (in which each workgroup updates a non-overlapping portion of the
// output tensors).
// This is possible because we control the production of such patterns in IREE
// and can take the necessary shortcuts wrt inplace semantics.
// In the future it is reasonable to expect special IR constructs to capture
// some of the destructive update patterns.
//
// Assumptions/Invariants on "Control the Production of Such Patterns"
// ===================================================================
// 1. Only output tensors may not be written by a destructive update pattern.
// 2. SubTensorOp/SubTensorInsertOp are the only ops that can extract/insert
//    from/into tensors.
// 3. All SubTensorOp/SubTensorInsertOp must have been introduced by Linalg
//    tiling on tensors.
// 4. Such tilings that result in yielded tensors across loops may only tile
//    parallel Linalg iterators atm.
// 5. (Future) Allow non-parallel Linalg iterators tiling and ensure first-read
//    or writeOnly by construction.
//
// The following destructive update patterns are rewritten.
//
// Coming from an `Flow::DispatchInputLoadOp`
// ==========================================
// ```
//   %0 = flow.dispatch.input.load %a : !flow.dispatch.input<...> -> tensor<...>
//   ...
//   %1 = destructive_update(%0)
//   ...
//   use_of(%1) // e.g. flow.dispatch.output.store %1, %b :
//              //        tensor<...> -> !flow.dispatch.output<...>
// ```
// is rewritten into:
// ```
//   %0 = flow.dispatch.input.load %a : !flow.dispatch.input<...> -> tensor<...>
//   ...
//   inplace_update(%0, %out) //e.g. flow.dispatch.output.store %subtensor, %b,
//                            //     offsets = ..., sizes = ..., strides = ... :
//                            //       tensor<...> -> !flow.dispatch.output<...>
//   %2 = flow.dispatch.output.load %b
//   ...
//   use_of(%2) // e.g. flow.dispatch.output.store %2, %b :
//              //        tensor<...> -> !flow.dispatch.output<...>
// ```
//
// This is a typical pattern that appears after tiling Linalg ops on tensors
// with operands that come from flow.dispatch.input/output.
//
// Other rewrites:
// ===============
// Furthermore, when no interleaved aliasing write to %b occurs, the following:
// ```
//   %2 = flow.dispatch.output.load %b
//   ...
//   flow.dispatch.output.store %2, %b :
//     tensor<...> -> !flow.dispatch.output<...>
// ```
// is elided.
LogicalResult rewriteLinalgDestructiveUpdates(
    IREE::Flow::DispatchWorkgroupsOp dispatchOp);

}  // namespace Flow
}  // namespace IREE
}  // namespace iree_compiler
}  // namespace mlir

#endif  // IREE_COMPILER_DIALECT_FLOW_TRANSFORMS_DESTRUCTIVEUPDATEUTILS_H_

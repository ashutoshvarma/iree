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

#ifndef IREE_COMPILER_DIALECT_HAL_IR_HALTYPES_H_
#define IREE_COMPILER_DIALECT_HAL_IR_HALTYPES_H_

#include <cstdint>

#include "iree/compiler/Dialect/IREE/IR/IREETypes.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringSwitch.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/TypeSupport.h"
#include "mlir/IR/Types.h"
#include "mlir/Support/LLVM.h"

// Order matters.
#include "iree/compiler/Dialect/HAL/IR/HALEnums.h.inc"
#include "iree/compiler/Dialect/HAL/IR/HALStructs.h.inc"

namespace mlir {
namespace iree_compiler {
namespace IREE {
namespace HAL {

#include "iree/compiler/Dialect/HAL/IR/HALOpInterface.h.inc"

//===----------------------------------------------------------------------===//
// Enum utilities
//===----------------------------------------------------------------------===//

// Returns a stable identifier for the MLIR element type or nullopt if the
// type is unsupported in the ABI.
llvm::Optional<int32_t> getElementTypeValue(Type type);

// Returns an attribute with the MLIR element type or {}.
IntegerAttr getElementTypeAttr(Type type);

// Returns the total bit count of elements of the given type.
size_t getElementBitCount(IntegerAttr elementType);
Value getElementBitCount(Location loc, Value elementType, OpBuilder &builder);

// Returns the rounded-up byte count of elements of the given type.
size_t getElementByteCount(IntegerAttr elementType);
Value getElementByteCount(Location loc, Value elementType, OpBuilder &builder);

//===----------------------------------------------------------------------===//
// Object types
//===----------------------------------------------------------------------===//

class AllocatorType : public Type::TypeBase<AllocatorType, Type, TypeStorage> {
 public:
  using Base::Base;
};

class BufferType : public Type::TypeBase<BufferType, Type, TypeStorage> {
 public:
  using Base::Base;
};

class BufferViewType
    : public Type::TypeBase<BufferViewType, Type, TypeStorage> {
 public:
  using Base::Base;
};

class CommandBufferType
    : public Type::TypeBase<CommandBufferType, Type, TypeStorage> {
 public:
  using Base::Base;
};

class DescriptorSetType
    : public Type::TypeBase<DescriptorSetType, Type, TypeStorage> {
 public:
  using Base::Base;
};

class DescriptorSetLayoutType
    : public Type::TypeBase<DescriptorSetLayoutType, Type, TypeStorage> {
 public:
  using Base::Base;
};

class DeviceType : public Type::TypeBase<DeviceType, Type, TypeStorage> {
 public:
  using Base::Base;
};

class EventType : public Type::TypeBase<EventType, Type, TypeStorage> {
 public:
  using Base::Base;
};

class ExecutableType
    : public Type::TypeBase<ExecutableType, Type, TypeStorage> {
 public:
  using Base::Base;
};

class ExecutableLayoutType
    : public Type::TypeBase<ExecutableLayoutType, Type, TypeStorage> {
 public:
  using Base::Base;
};

class RingBufferType
    : public Type::TypeBase<RingBufferType, Type, TypeStorage> {
 public:
  using Base::Base;
};

class SemaphoreType : public Type::TypeBase<SemaphoreType, Type, TypeStorage> {
 public:
  using Base::Base;
};

//===----------------------------------------------------------------------===//
// Struct types
//===----------------------------------------------------------------------===//

// Returns the intersection (most conservative) constraints |lhs| ∩ |rhs|.
BufferConstraintsAttr intersectBufferConstraints(BufferConstraintsAttr lhs,
                                                 BufferConstraintsAttr rhs);

// TODO(benvanik): runtime buffer constraint queries from the allocator.
// We can add folders for those when the allocator is strongly-typed with
// #hal.buffer_constraints and otherwise leave them for runtime queries.
class BufferConstraintsAdaptor {
 public:
  BufferConstraintsAdaptor(Location loc, Value allocator);

  Value getMaxAllocationSize(OpBuilder &builder);
  Value getMinBufferOffsetAlignment(OpBuilder &builder);
  Value getMaxBufferRange(OpBuilder &builder);
  Value getMinBufferRangeAlignment(OpBuilder &builder);

 private:
  Location loc_;
  Value allocator_;
  BufferConstraintsAttr bufferConstraints_;
};

class BufferBarrierType {
 public:
  static TupleType get(MLIRContext *context) {
    return TupleType::get(context, {
                                       IntegerType::get(context, 32),
                                       IntegerType::get(context, 32),
                                       BufferType::get(context),
                                       IndexType::get(context),
                                       IndexType::get(context),
                                   });
  }
};

class MemoryBarrierType {
 public:
  static TupleType get(MLIRContext *context) {
    return TupleType::get(context, {
                                       IntegerType::get(context, 32),
                                       IntegerType::get(context, 32),
                                   });
  }
};

// A tuple containing runtime values for a descriptor set binding:
// <binding ordinal, hal.buffer, buffer byte offset, buffer byte length>
using DescriptorSetBindingValue = std::tuple<uint32_t, Value, Value, Value>;

}  // namespace HAL
}  // namespace IREE
}  // namespace iree_compiler
}  // namespace mlir

#endif  // IREE_COMPILER_DIALECT_HAL_IR_HALTYPES_H_

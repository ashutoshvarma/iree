// Copyright 2020 Google LLC
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

#ifndef IREE_DIALECT_VMLA_BASE
#define IREE_DIALECT_VMLA_BASE

include "iree/compiler/Dialect/IREE/IR/IREEBase.td"
include "iree/compiler/Dialect/Shape/IR/ShapeBase.td"

//===----------------------------------------------------------------------===//
// IREE VMLA (Virtual Machine-based Linear Algebra) dialect
//===----------------------------------------------------------------------===//

def VMLA_Dialect : Dialect {
  let name = "vmla";
  let cppNamespace = "::mlir::iree_compiler::IREE::VMLA";

  let summary = [{
    A dialect representing operations against the IREE VM-based backend.
  }];
  let description = [{
    This is a reference dialect representing a simple IREE VM-based linear
    algebra module that is used as a library at runtime. The ops in this dialect
    map (roughly) 1:1 with the exported functions in the runtime module.

    See `vmla.imports.mlir` for the full list of exported functions.
  }];
}

//===----------------------------------------------------------------------===//
// VMLA enums
//===----------------------------------------------------------------------===//

def VMLA_CmpPredicate_EQ : I32EnumAttrCase<"EQ", 0>;
def VMLA_CmpPredicate_NE : I32EnumAttrCase<"NE", 1>;
def VMLA_CmpPredicate_LT : I32EnumAttrCase<"LT", 2>;
def VMLA_CmpPredicate_LE : I32EnumAttrCase<"LE", 3>;
def VMLA_CmpPredicate_GT : I32EnumAttrCase<"GT", 4>;
def VMLA_CmpPredicate_GE : I32EnumAttrCase<"GE", 5>;
def VMLA_CmpPredicateAttr :
    I32EnumAttr<"CmpPredicate", "IREE VMLA comparison op predicate", [
      VMLA_CmpPredicate_EQ,
      VMLA_CmpPredicate_NE,
      VMLA_CmpPredicate_LT,
      VMLA_CmpPredicate_LE,
      VMLA_CmpPredicate_GT,
      VMLA_CmpPredicate_GE,
    ]> {
  let cppNamespace = "::mlir::iree_compiler::IREE::VMLA";
}

//===----------------------------------------------------------------------===//
// VMLA types
//===----------------------------------------------------------------------===//

def VMLA_DeviceSize : TypeAlias<Index>;
def VMLA_DeviceSizeAttr : IREE_IndexAttrBase<"size_t">;

def VMLA_HostSize : TypeAlias<Index>;
def VMLA_HostSizeAttr : IREE_IndexAttrBase<"size_t">;

def VMLA_Index : TypeAlias<Index>;

def VMLA_Shape : TypeAlias<Shape_RankedShape>;

def VMLA_HostBuffer : AnyTypeOf<[
  ByteBufferType,
  MutableByteBufferType,
]>;

def VMLA_Buffer : DialectType<
    VMLA_Dialect,
    CPred<"$_self.isa<IREE::VMLA::BufferType>()">,
    "buffer"> {
  let description = [{
    A lightweight unshaped byte buffer.
  }];
  let builderCall = "$_builder.getType<IREE::VMLA::BufferType>()";
}

def VMLA_Interface : DialectType<
    VMLA_Dialect,
    CPred<"$_self.isa<IREE::VMLA::InterfaceType>()">,
    "interface"> {
  let description = [{
    Binding and parameter interface (derived from `hal.interface`).
  }];

  let builderCall = "$_builder.getType<IREE::VMLA::InterfaceType>()";
}

// TODO(benvanik): figure out how to get constraints to work.
// def VMLA_AnyTypeAttr : Confined<TypeAttr, [I8Attr, I16Attr, I32Attr, F32Attr]> {
//   let constBuilderCall = ?;
//   let defaultValue = ?;
// }
// def VMLA_FloatTypeAttr : Confined<TypeAttr, [F32Attr]> {
//   let constBuilderCall = ?;
//   let defaultValue = ?;
// }
def VMLA_AnyTypeAttr : TypeAttrBase<"Type", "any type attribute">;
def VMLA_FloatTypeAttr : TypeAttrBase<"Type", "any type attribute">;

//===----------------------------------------------------------------------===//
// VMLA op traits
//===----------------------------------------------------------------------===//

// Operations with this trait require shapes be provided for all buffers.
// For example, if the original HLO op had an `%arg : tensor<4x?xf32>`, adding
// this trait will have the converted op contain both a `%arg : !vmla.buffer`
// and an `%arg_shape : shapex.ranked_shape<[4,?]>`.
def VMLA_IncludeShapes : NativeOpTrait<"IREE::VMLA::IncludeShapes">;

//===----------------------------------------------------------------------===//
// Base VMLA op classes
//===----------------------------------------------------------------------===//

def VMLA_OpInterface : OpInterface<"VMLAOp"> {
  let description = [{
    Interface for VMLA ops that can be used to customize the lowering.
    This is required as there is not a way to get reflection information about
    ops.
  }];

  let methods = [
    StaticInterfaceMethod<
      "Extracts type information attributes that may be required by the op.",
      "void", "extractTypeAttributes", (ins "OperationState &":$state, "ArrayRef<Type>":$operandTypes, "ArrayRef<Type>":$resultTypes), [{
        ConcreteOp::extractTypeAttributes(state, operandTypes, resultTypes);
      }], [{
        return;  // default to no extraction
      }]
    >,
  ];
}

class VMLA_Op<string mnemonic, list<OpTrait> traits = []> :
    Op<VMLA_Dialect, mnemonic, !listconcat(traits, [])> {
  // TODO(benvanik): use new tablegen printer/parser.
  // let parser = [{ return parse$cppClass(parser, &result); }];
  // let printer = [{ return print$cppClass(p, *this); }];
}

class VMLA_ElementTypeOp<string mnemonic, list<OpTrait> traits = []> :
    VMLA_Op<mnemonic, !listconcat(traits, [VMLA_OpInterface])> {
  let extraClassDeclaration = [{
    static void extractTypeAttributes(OperationState &state, ArrayRef<Type> operandTypes, ArrayRef<Type> resultTypes) {
      state.addAttribute("element_type", TypeAttr::get(resultTypes[0].cast<ShapedType>().getElementType()));
    }
  }];
}

class VMLA_UnaryOp<string mnemonic, Attr typeAttr, list<OpTrait> traits = []> :
    VMLA_ElementTypeOp<mnemonic, traits> {
  let arguments = (ins
    VMLA_Buffer:$src,
    VMLA_Buffer:$dst,
    typeAttr:$element_type,
    // TODO(benvanik): remove once unsigned types are in MLIR.
    UnitAttr:$forceUnsigned
  );

  let assemblyFormat = "$src`,` `out` $dst attr-dict `:` $element_type";
}

class VMLA_BinaryOp<string mnemonic, Attr typeAttr, list<OpTrait> traits = []>
    : VMLA_ElementTypeOp<mnemonic, traits> {
  let arguments = (ins
    VMLA_Buffer:$lhs,
    VMLA_Buffer:$rhs,
    VMLA_Buffer:$dst,
    typeAttr:$element_type,
    // TODO(benvanik): remove once unsigned types are in MLIR.
    UnitAttr:$forceUnsigned
  );

  let assemblyFormat = "$lhs`,` $rhs`,` `out` $dst attr-dict `:` $element_type";
}

class VMLA_BinaryBroadcastOp<string mnemonic, Attr typeAttr, list<OpTrait> traits = []>
    : VMLA_ElementTypeOp<mnemonic, traits> {
  let arguments = (ins
    VMLA_Buffer:$lhs,
    I32:$rhs,
    VMLA_Buffer:$dst,
    typeAttr:$element_type,
    // TODO(benvanik): remove once unsigned types are in MLIR.
    UnitAttr:$forceUnsigned
  );

  let assemblyFormat = "$lhs`,` $rhs`,` `out` $dst attr-dict `:` $element_type";
}

class VMLA_TernaryOp<string mnemonic, Attr typeAttr, list<OpTrait> traits = []>
    : VMLA_ElementTypeOp<mnemonic, traits> {
  let arguments = (ins
    VMLA_Buffer:$a,
    VMLA_Buffer:$b,
    VMLA_Buffer:$c,
    VMLA_Buffer:$dst,
    typeAttr:$element_type,
    // TODO(benvanik): remove once unsigned types are in MLIR.
    UnitAttr:$forceUnsigned
  );

  let assemblyFormat = "$a`,` $b`,` $c`,` `out` $dst attr-dict `:` $element_type";
}

#endif  // IREE_DIALECT_VMLA_BASE

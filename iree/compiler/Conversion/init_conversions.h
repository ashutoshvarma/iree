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

#ifndef IREE_COMPILER_CONVERSION_INIT_CONVERSIONS_H_
#define IREE_COMPILER_CONVERSION_INIT_CONVERSIONS_H_

#include "iree/compiler/Conversion/CodegenUtils/ForOpCanonicalization.h"
#include "iree/compiler/Conversion/Common/Passes.h"
#include "iree/compiler/Conversion/HLOToHLO/Passes.h"
#include "iree/compiler/Conversion/HLOToLinalg/HLOToLinalgOnTensorPasses.h"
#include "iree/compiler/Conversion/HLOToLinalg/Passes.h"
#include "iree/compiler/Conversion/LinalgToLLVM/Passes.h"
#include "iree/compiler/Conversion/LinalgToSPIRV/Passes.h"
#include "iree/compiler/Conversion/LinalgToVector/Passes.h"

namespace mlir {
namespace iree_compiler {

// These functions should be called before creating any MLIRContext if one
// expects all the possible conversions to be made available to the context
// automatically.

inline void registerCommonConversionPasses() {
  static bool init_once = []() {
    // Common
    createLinalgBufferizePass();
    createLinalgRewriteDestructiveUpdatesPass();
    return true;
  }();
  (void)init_once;
}

inline void registerHLOToLinalgPasses() {
  createDecomposeHLOClampPass();
  createHLOToLinalgOnBuffersPass();
  createHLOToLinalgOnTensorsPass();
  createDemoteF32ToF16Pass();
}

inline void registerLinalgToVectorPasses() {
  static bool init_once = []() {
    createLoadStoreVectorizationPass();
    return true;
  }();
  (void)init_once;
}

inline void registerLinalgToSPIRVPasses() {
  static bool init_once = []() {
    // LinalgToSPIRV
    createConvertToGPUPass(SPIRVCodegenOptions());
    createFoldProcessorIDUsesPass();
    createLinalgTileAndFusePass(SPIRVCodegenOptions());
    createSplitDispatchFunctionPass();
    createVectorToGPUPass();
    createMatMulTileAndVectorizeGPUPass();
    createForOpCanonicalizationPass();
    createVectorizeMemref();
    return true;
  }();
  (void)init_once;
}

inline void registerLinalgToLLVMPasses() {
  static bool init_once = []() {
    // LinalgToLLVM
    createConvImg2ColMatmulConversionPass();
    createLinalgTileAndDistributePass();
    createLinalgTileAndDistributeOnTensorsPass();
    createLinalgTileAndVectorizeWorkgroupsPass();
    return true;
  }();
  (void)init_once;
}

}  // namespace iree_compiler
}  // namespace mlir

#endif  // IREE_COMPILER_CONVERSION_INIT_CONVERSIONS_H_

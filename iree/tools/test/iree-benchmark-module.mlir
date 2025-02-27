// RUN: iree-translate --iree-hal-target-backends=vmla -iree-mlir-to-vm-bytecode-module %s | iree-benchmark-module --driver=vmla --entry_function=abs --function_inputs="i32=-2" | IreeFileCheck %s
// RUN: [[ $IREE_VULKAN_DISABLE == 1 ]] || (iree-translate --iree-hal-target-backends=vulkan-spirv -iree-mlir-to-vm-bytecode-module %s | iree-benchmark-module --driver=vulkan --entry_function=abs --function_inputs="i32=-2" | IreeFileCheck %s)
// RUN: [[ $IREE_LLVMAOT_DISABLE == 1 ]] || (iree-translate --iree-hal-target-backends=dylib-llvm-aot -iree-mlir-to-vm-bytecode-module %s | iree-benchmark-module --driver=dylib --entry_function=abs --function_inputs="i32=-2" | IreeFileCheck %s)

// CHECK-LABEL: BM_abs
func @abs(%input : tensor<i32>) -> (tensor<i32>) attributes { iree.module.export } {
  %result = "mhlo.abs"(%input) : (tensor<i32>) -> tensor<i32>
  return %result : tensor<i32>
}

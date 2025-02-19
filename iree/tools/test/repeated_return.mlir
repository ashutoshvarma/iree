// RUN: (iree-translate --iree-hal-target-backends=vmla -iree-mlir-to-vm-bytecode-module %s | iree-run-module --entry_function=many_tensor) | IreeFileCheck %s
// RUN: iree-translate --iree-hal-target-backends=vmla -iree-mlir-to-vm-bytecode-module %s | iree-benchmark-module --driver=vmla --entry_function=many_tensor | IreeFileCheck --check-prefix=BENCHMARK %s
// RUN: iree-run-mlir -export-all -iree-hal-target-backends=vmla %s | IreeFileCheck %s

// BENCHMARK-LABEL: BM_many_tensor
// CHECK-LABEL: EXEC @many_tensor
func @many_tensor() -> (tensor<2x2xf32>, tensor<2x2xf32>, tensor<2x2xf32>,
                        tensor<2x2xf32>, tensor<2x2xf32>, tensor<2x2xf32>) attributes { iree.module.export } {
  %res = iree.unfoldable_constant
      dense<[[1.0, 2.0], [3.0, 4.0]]> : tensor<2x2xf32>
  return %res, %res, %res, %res, %res, %res :
        tensor<2x2xf32>, tensor<2x2xf32>, tensor<2x2xf32>, tensor<2x2xf32>,
        tensor<2x2xf32>, tensor<2x2xf32>
}
// CHECK-COUNT-6: 2x2xf32=[1 2][3 4]

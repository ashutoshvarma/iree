func @tensor_float() attributes { iree.module.export } {
  %0 = iree.unfoldable_constant dense<[1.0, -1.5, 7.0, -2.0]> : tensor<4xf32>
  %1 = iree.unfoldable_constant dense<[5.0, 1.0, 7.0, -3.0]> : tensor<4xf32>
  %result = "tosa.greater_equal"(%0, %1) : (tensor<4xf32>, tensor<4xf32>) -> tensor<4xi1>
  check.expect_eq_const(%result, dense<[false, false, true, true]> : tensor<4xi1>) : tensor<4xi1>
  return
}

func @tensor_int() attributes { iree.module.export } {
  %0 = iree.unfoldable_constant dense<[1, 0, 5, 3]> : tensor<4xi32>
  %1 = iree.unfoldable_constant dense<[5, 0, 1, 8]> : tensor<4xi32>
  %result = "tosa.greater_equal"(%0, %1) : (tensor<4xi32>, tensor<4xi32>) -> tensor<4xi1>
  check.expect_eq_const(%result, dense<[false, true, true, false]> : tensor<4xi1>) : tensor<4xi1>
  return
}

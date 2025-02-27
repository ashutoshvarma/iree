func @f32() attributes { iree.module.export } {
  %0 = iree.unfoldable_constant dense<[1.0, 6.0, -6.0, 0.0]> : tensor<4xf32>
  %1 = iree.unfoldable_constant dense<[0.0, 2.0, 3.0, 4.0]> : tensor<4xf32>
  %2 = "mhlo.divide"(%0, %1) : (tensor<4xf32>, tensor<4xf32>) -> tensor<4xf32>
  %result = "mhlo.is_finite"(%2) : (tensor<4xf32>) -> tensor<4xi1>
  %c0 = iree.unfoldable_constant dense<0> : tensor<4xi8>
  %c1 = iree.unfoldable_constant dense<1> : tensor<4xi8>
  %output = "mhlo.select"(%result, %c1, %c0) : (tensor<4xi1>, tensor<4xi8>, tensor<4xi8>) -> tensor<4xi8>
  check.expect_eq_const(%output, dense<[0, 1, 1, 1]> : tensor<4xi8>) : tensor<4xi8>
  return
}

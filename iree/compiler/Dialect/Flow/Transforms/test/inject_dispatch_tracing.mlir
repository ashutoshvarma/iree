// RUN: iree-opt -split-input-file -pass-pipeline='func(iree-flow-inject-dispatch-tracing)' %s | IreeFileCheck %s

// CHECK-LABEL: func @singleDispatch
// CHECK-SAME: (%[[ARG0:.+]]: tensor<4xf32>)
func @singleDispatch(%arg0: tensor<4xf32>) -> tensor<4xf32> {
  %c4 = constant 4 : index
  //      CHECK: flow.tensor.trace {key = "ex::entry0 inputs"} %[[ARG0]] : tensor<4xf32>
  // CHECK-NEXT: %[[RET0:.+]] = flow.dispatch @ex::@entry0[%c4] (%[[ARG0]]) : (tensor<4xf32>) -> tensor<4xf32>
  %0 = flow.dispatch @ex::@entry0[%c4] (%arg0) : (tensor<4xf32>) -> tensor<4xf32>
  // CHECK-NEXT: flow.tensor.trace {key = "ex::entry0 outputs"} %[[RET0]] : tensor<4xf32>
  // CHECK-NEXT: return %[[RET0]]
  return %0 : tensor<4xf32>
}

// -----

// CHECK-LABEL: func @multiDispatch
// CHECK-SAME: (%[[ARG0:.+]]: tensor<4xf32>)
func @multiDispatch(%arg0: tensor<4xf32>) -> tensor<4xf32> {
  %c4 = constant 4 : index

  //     CHECK: flow.tensor.trace {key = "ex::entry0 inputs"} %[[ARG0]] : tensor<4xf32>
  // CHECK-NEXT: %[[RET0:.+]] = flow.dispatch @ex::@entry0[%c4] (%[[ARG0]]) : (tensor<4xf32>) -> tensor<4xf32>
  %0 = flow.dispatch @ex::@entry0[%c4] (%arg0) : (tensor<4xf32>) -> tensor<4xf32>
  // CHECK-NEXT: flow.tensor.trace {key = "ex::entry0 outputs"} %[[RET0]] : tensor<4xf32>

  //     CHECK: flow.tensor.trace {key = "ex::entry1 inputs"} %[[RET0]] : tensor<4xf32>
  // CHECK-NEXT: %[[RET1:.+]] = flow.dispatch @ex::@entry1[%c4] (%[[RET0]]) : (tensor<4xf32>) -> tensor<4xf32>
  %1 = flow.dispatch @ex::@entry1[%c4] (%0) : (tensor<4xf32>) -> tensor<4xf32>
  // CHECK-NEXT: flow.tensor.trace {key = "ex::entry1 outputs"} %[[RET1]] : tensor<4xf32>

  // CHECK: return %[[RET1]]
  return %1 : tensor<4xf32>
}

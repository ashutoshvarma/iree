# Lint as: python3
# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Test matrix ops."""

from absl import app
from pyiree.tf.support import tf_test_utils
from pyiree.tf.support import tf_utils
import tensorflow.compat.v2 as tf


class MatrixOpsDynamicModule(tf.Module):

  @tf.function(input_signature=[
      tf.TensorSpec([None, None, 4, 2], tf.float32),
      tf.TensorSpec([None, None, 2, 4], tf.float32),
  ])
  def matmul_high_rank_batch(self, lhs, rhs):
    return tf.matmul(lhs, rhs)

  @tf.function(input_signature=[
      tf.TensorSpec([None, None, None], tf.float32),
      tf.TensorSpec([None, None, None], tf.float32),
  ])
  def matmul_dynamic(self, lhs, rhs):
    return tf.matmul(lhs, rhs)

  @tf.function(input_signature=[
      tf.TensorSpec([None, None, None], tf.float32),
      tf.TensorSpec([None, None], tf.float32),
  ])
  def matmul_dynamic_lhs_batch(self, lhs, rhs):
    return tf.matmul(lhs, rhs)


class MatrixOpsDynamicTest(tf_test_utils.TracedModuleTestCase):

  def __init__(self, *args, **kwargs):
    super().__init__(*args, **kwargs)
    self._modules = tf_test_utils.compile_tf_module(MatrixOpsDynamicModule)

  # yapf: disable
  def test_matmul_high_rank_batch(self):
    def matmul_high_rank_batch(module):
      module.matmul_high_rank_batch(
          tf_utils.uniform([1, 7, 4, 2]), tf_utils.uniform([7, 1, 2, 4]))
    self.compare_backends(matmul_high_rank_batch, self._modules)

  def test_matmul_dynamic_matching_batch(self):
    def matmul_dynamic_matching_batch(module):
      module.matmul_dynamic(
          tf_utils.uniform([2, 2, 3]), tf_utils.uniform([2, 3, 4]))
    self.compare_backends(matmul_dynamic_matching_batch, self._modules)

  def test_matmul_dynamic_broadcast_lhs(self):
    def matmul_dynamic_broadcast_lhs(module):
      module.matmul_dynamic(
          tf_utils.uniform([1, 2, 3]), tf_utils.uniform([2, 3, 4]))
    self.compare_backends(matmul_dynamic_broadcast_lhs, self._modules)

  def test_matmul_dynamic_broadcast_rhs(self):
    def matmul_dynamic_broadcast_rhs(module):
      module.matmul_dynamic(
          tf_utils.uniform([2, 2, 3]), tf_utils.uniform([1, 3, 4]))
    self.compare_backends(matmul_dynamic_broadcast_rhs, self._modules)

  def test_matmul_dynamic_rank_broadcasting(self):
    def matmul_dynamic_rank_broadcasting(module):
      module.matmul_dynamic_lhs_batch(
          tf_utils.uniform([7, 2, 3]), tf_utils.uniform([3, 4]))
    self.compare_backends(matmul_dynamic_rank_broadcasting, self._modules)
  # yapf: enable


def main(argv):
  del argv  # Unused
  if hasattr(tf, 'enable_v2_behavior'):
    tf.enable_v2_behavior()
  tf.test.main()


if __name__ == '__main__':
  app.run(main)

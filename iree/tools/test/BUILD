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

# Smoke tests for the execution of tool binaries.

load("//iree:lit_test.bzl", "iree_lit_test_suite")

package(
    default_visibility = ["//visibility:public"],
    features = ["layering_check"],
    licenses = ["notice"],  # Apache 2.0
)

iree_lit_test_suite(
    name = "lit",
    srcs = glob(["*.mlir"]),
    data = [
        "//iree/tools:IreeFileCheck",
        "//iree/tools:iree-benchmark-module",
        "//iree/tools:iree-run-mlir",
        "//iree/tools:iree-run-module",
        "//iree/tools:iree-translate",
    ],
    tags = ["hostonly"],
)

iree_lit_test_suite(
    name = "benchmark_flags",
    srcs = ["benchmark_flags.txt"],
    data = [
        "//iree/tools:IreeFileCheck",
        "//iree/tools:iree-benchmark-module",
        "//iree/tools:iree-translate",
    ],
    tags = ["hostonly"],
)

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

iree_add_all_subdirs()

iree_cc_library(
  NAME
    cts_test_base
  HDRS
    "cts_test_base.h"
  DEPS
    iree::base::api
    iree::hal::api
    iree::testing::gtest
  TESTONLY
  PUBLIC
)

iree_cc_test(
  NAME
    allocator_test
  SRCS
    "allocator_test.cc"
  DEPS
    ::cts_test_base
    iree::hal::testing::driver_registry
    iree::testing::gtest
    iree::testing::gtest_main
)

iree_cc_test(
  NAME
    buffer_mapping_test
  SRCS
    "buffer_mapping_test.cc"
  DEPS
    ::cts_test_base
    iree::hal::testing::driver_registry
    iree::testing::gtest
    iree::testing::gtest_main
)

iree_cc_test(
  NAME
    command_buffer_test
  SRCS
    "command_buffer_test.cc"
  DEPS
    ::cts_test_base
    iree::hal::testing::driver_registry
    iree::testing::gtest
    iree::testing::gtest_main
)

iree_cc_test(
  NAME
    descriptor_set_test
  SRCS
    "descriptor_set_test.cc"
  DEPS
    ::cts_test_base
    iree::hal::testing::driver_registry
    iree::testing::gtest
    iree::testing::gtest_main
)

iree_cc_test(
  NAME
    descriptor_set_layout_test
  SRCS
    "descriptor_set_layout_test.cc"
  DEPS
    ::cts_test_base
    iree::hal::testing::driver_registry
    iree::testing::gtest
    iree::testing::gtest_main
)

iree_cc_test(
  NAME
    driver_test
  SRCS
    "driver_test.cc"
  DEPS
    ::cts_test_base
    iree::hal::testing::driver_registry
    iree::testing::gtest
    iree::testing::gtest_main
)

iree_cc_test(
  NAME
    event_test
  SRCS
    "event_test.cc"
  DEPS
    ::cts_test_base
    iree::hal::testing::driver_registry
    iree::testing::gtest
    iree::testing::gtest_main
)

iree_cc_test(
  NAME
    executable_layout_test
  SRCS
    "executable_layout_test.cc"
  DEPS
    ::cts_test_base
    iree::hal::testing::driver_registry
    iree::testing::gtest
    iree::testing::gtest_main
)

iree_cc_test(
  NAME
    semaphore_test
  SRCS
    "semaphore_test.cc"
  DEPS
    ::cts_test_base
    iree::hal::testing::driver_registry
    iree::testing::gtest
    iree::testing::gtest_main
)

iree_cc_test(
  NAME
    semaphore_submission_test
  SRCS
    "semaphore_submission_test.cc"
  DEPS
    ::cts_test_base
    iree::hal::testing::driver_registry
    iree::testing::gtest
    iree::testing::gtest_main
)

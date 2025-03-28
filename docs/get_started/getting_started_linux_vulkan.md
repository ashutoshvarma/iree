# Getting Started on Linux with Vulkan

[Vulkan](https://www.khronos.org/vulkan/) is a new generation graphics and
compute API that provides high-efficiency, cross-platform access to modern GPUs
used in a wide variety of devices from PCs and consoles to mobile phones and
embedded platforms.

IREE includes a Vulkan/[SPIR-V](https://www.khronos.org/registry/spir-v/) HAL
backend designed for executing advanced ML models in a deeply pipelined and
tightly integrated fashion on accelerators like GPUs.

This guide will walk you through using IREE's compiler and runtime Vulkan
components. For generic Vulkan development environment set up and trouble
shooting, please see [this doc](generic_vulkan_env_setup.md).

## Prerequisites

You should already have IREE cloned and building on your Linux machine. See the
[Getting Started on Linux with CMake](getting_started_linux_cmake.md) or
[Getting Started on Linux with Bazel](getting_started_linux_bazel.md) guide for
instructions.

You may have a physical GPU with drivers supporting Vulkan. We also support
using [SwiftShader](https://swiftshader.googlesource.com/SwiftShader/) (a high
performance CPU-based implementation of Vulkan).

Vulkan drivers implementing API version >= 1.2 are recommended. IREE requires
the `VK_KHR_timeline_semaphore` extension (part of Vulkan 1.2), though it is
able to emulate it, with performance costs, as necessary.

## Vulkan Setup

### Background

Please see
[Generic Vulkan Development Environment Setup and Troubleshooting](generic_vulkan_env_setup.md)
for generic Vulkan concepts and development environment setup.

### Quick Start

The
[dynamic_symbols_test](https://github.com/google/iree/blob/main/iree/hal/vulkan/dynamic_symbols_test.cc)
checks if the Vulkan loader and a valid ICD are accessible.

Run the test:

```shell
# -- CMake --
$ export VK_LOADER_DEBUG=all
$ cmake --build ../iree-build/ --target iree_hal_vulkan_dynamic_symbols_test
$ ../iree-build/iree/hal/vulkan/iree_hal_vulkan_dynamic_symbols_test

# -- Bazel --
$ bazel test iree/hal/vulkan:dynamic_symbols_test --test_env=VK_LOADER_DEBUG=all
```

Tests in IREE's HAL "Conformance Test Suite" (CTS) actually exercise the Vulkan
HAL, which includes checking for supported layers and extensions.

Run the
[driver test](https://github.com/google/iree/blob/main/iree/hal/cts/driver_test.cc):

```shell
# -- CMake --
$ export VK_LOADER_DEBUG=all
$ cmake --build ../iree-build/ --target iree_hal_cts_driver_test
$ ../iree-build/iree/hal/cts/iree_hal_cts_driver_test

# -- Bazel --
$ bazel test iree/hal/cts:driver_test --test_env=VK_LOADER_DEBUG=all --test_output=all
```

If these tests pass, you can skip down to the next section.

### Setting up SwiftShader

If your system lacks a physical GPU with compatible Vulkan drivers, or you just
want to use a software driver for predictable performance, you can set up
SwiftShader's Vulkan ICD (Installable Client Driver).

IREE has a
[helper script](https://github.com/google/iree/blob/main/build_tools/third_party/swiftshader/build_vk_swiftshader.sh)
for building SwiftShader from source using CMake:

```shell
$ bash build_tools/third_party/swiftshader/build_vk_swiftshader.sh
```

<!-- TODO(scotttodd): Steps to download prebuilt binaries when they exist -->

After building, the script will prompt your to add a variable `VK_ICD_FILENAMES`
to your environment to tell the Vulkan loader to use the ICD. Assuming it was
installed in the default location, this can be done via:

```shell
$ export VK_ICD_FILENAMES="${HOME?}/.swiftshader/Linux/vk_swiftshader_icd.json"
```

### Support in Bazel Tests

Bazel tests run in a sandbox, which environment variables may be forwarded to
using the `--test_env` flag. A user.bazelrc file supporting each of the steps
above looks like this (substitute for the `{}` paths):

```
test --test_env="LD_LIBRARY_PATH={PATH_TO_VULKAN_SDK}/x86_64/lib/"
test --test_env="LD_PRELOAD=libvulkan.so.1"
test --test_env="VK_ICD_FILENAMES={PATH_TO_IREE}/build-swiftshader/Linux/vk_swiftshader_icd.json"
```

## Using IREE's Vulkan Compiler Target and Runtime Driver

### Compiling for the Vulkan HAL

Pass the flag `-iree-hal-target-backends=vulkan-spirv` to `iree-translate`:

```shell
# -- CMake --
$ cmake --build ../iree-build/ --target iree_tools_iree-translate
$ ../iree-build/iree/tools/iree-translate -iree-mlir-to-vm-bytecode-module -iree-hal-target-backends=vulkan-spirv ./iree/tools/test/simple.mlir -o /tmp/module.vmfb

# -- Bazel --
$ bazel run iree/tools:iree-translate -- -iree-mlir-to-vm-bytecode-module -iree-hal-target-backends=vulkan-spirv $PWD/iree/tools/test/simple.mlir -o /tmp/module.vmfb
```

> Tip:<br>
> &nbsp;&nbsp;&nbsp;&nbsp;If successful, this may have no output. You can pass
> other flags like `-print-ir-after-all` to control the program.

### Executing modules with the Vulkan driver

Pass the flag `-driver=vulkan` to `iree-run-module`:

```shell
# -- CMake --
$ cmake --build ../iree-build/ --target iree_tools_iree-run-module
$ ../iree-build/iree/tools/iree-run-module -module_file=/tmp/module.vmfb -driver=vulkan -entry_function=abs -function_inputs="i32=-2"

# -- Bazel --
$ bazel run iree/tools:iree-run-module -- -module_file=/tmp/module.vmfb -driver=vulkan -entry_function=abs -function_inputs="i32=-2"
```

## Running IREE's Vulkan Samples

> Note:<br>
> &nbsp;&nbsp;&nbsp;&nbsp;The Vulkan samples are CMake-only.

Install the [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/), then run:

```shell
$ cmake --build ../iree-build/ --target iree_samples_vulkan_vulkan_inference_gui
$ ../iree-build/iree/samples/vulkan/vulkan_inference_gui
```

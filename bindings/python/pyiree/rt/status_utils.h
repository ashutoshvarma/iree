// Copyright 2019 Google LLC
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

#ifndef IREE_BINDINGS_PYTHON_PYIREE_COMMON_STATUS_UTILS_H_
#define IREE_BINDINGS_PYTHON_PYIREE_COMMON_STATUS_UTILS_H_

#include "iree/base/api.h"
#include "pybind11/pybind11.h"

namespace iree {
namespace python {

// Raises a value error with the given message.
// Correct usage:
//   throw RaiseValueError(PyExc_ValueError, "Foobar'd");
pybind11::error_already_set RaisePyError(PyObject* exc_class,
                                         const char* message);

// Raises a value error with the given message.
// Correct usage:
//   throw RaiseValueError("Foobar'd");
inline pybind11::error_already_set RaiseValueError(const char* message) {
  return RaisePyError(PyExc_ValueError, message);
}

pybind11::error_already_set ApiStatusToPyExc(iree_status_t status,
                                             const char* message);

inline void CheckApiStatus(iree_status_t status, const char* message) {
  if (iree_status_is_ok(status)) {
    return;
  }
  throw ApiStatusToPyExc(status, message);
}

inline void CheckApiNotNull(const void* p, const char* message) {
  if (!p) {
    throw RaiseValueError(message);
  }
}

}  // namespace python
}  // namespace iree

#endif  // IREE_BINDINGS_PYTHON_PYIREE_COMMON_STATUS_UTILS_H_

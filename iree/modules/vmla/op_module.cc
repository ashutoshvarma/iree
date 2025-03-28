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

#include "iree/modules/vmla/op_module.h"

#include <cstdint>

#include "absl/types/span.h"
#include "iree/base/tracing.h"
#include "iree/modules/vmla/op_kernels.h"
#include "iree/vm/module_abi_packing.h"

//===----------------------------------------------------------------------===//
// Type registration
//===----------------------------------------------------------------------===//

static iree_vm_ref_type_descriptor_t Buffer_descriptor = {0};
static iree_vm_ref_type_descriptor_t Interface_descriptor = {0};

IREE_VM_DEFINE_TYPE_ADAPTERS(Buffer, iree::hal::vmla::Buffer);
IREE_VM_DEFINE_TYPE_ADAPTERS(Interface, iree::hal::vmla::Interface);

#define IREE_VMLA_REGISTER_CC_TYPE(type, name, descriptor)     \
  descriptor.type_name = iree_make_cstring_view(name);         \
  descriptor.offsetof_counter = type::offsetof_counter();      \
  descriptor.destroy = type::DirectDestroy;                    \
  IREE_RETURN_IF_ERROR(iree_vm_ref_register_type(&descriptor), \
                       "failed to register type %s", name);

namespace iree {
namespace hal {
namespace vmla {

Status ModuleRegisterTypes() {
  static bool has_registered = false;
  if (has_registered) return OkStatus();

  IREE_VMLA_REGISTER_CC_TYPE(Buffer, "vmla.buffer", Buffer_descriptor);
  IREE_VMLA_REGISTER_CC_TYPE(Interface, "vmla.interface", Interface_descriptor);

  has_registered = true;
  return OkStatus();
}

//===----------------------------------------------------------------------===//
// API type implementations
//===----------------------------------------------------------------------===//

// static
StatusOr<vm::ref<Buffer>> Buffer::Allocate(size_t byte_length,
                                           iree_allocator_t allocator) {
  void* data = nullptr;
  IREE_RETURN_IF_ERROR(iree_allocator_malloc(allocator, byte_length, &data),
                       "failed to allocate buffer of size %zu", byte_length);

  auto buffer = vm::assign_ref(new Buffer());
  buffer->data_ = data;
  buffer->data_length_ = byte_length;
  buffer->allocator_ = allocator;
  return std::move(buffer);
}

// static
StatusOr<vm::ref<Buffer>> Buffer::Wrap(const void* data, size_t data_length,
                                       iree_allocator_t allocator) {
  auto buffer = vm::assign_ref(new Buffer());
  buffer->data_ = const_cast<void*>(data);
  buffer->data_length_ = data_length;
  buffer->allocator_ = allocator;
  return std::move(buffer);
}

// static
StatusOr<vm::ref<Buffer>> Buffer::WrapMutable(void* data, size_t data_length,
                                              iree_allocator_t allocator) {
  auto buffer = vm::assign_ref(new Buffer());
  buffer->data_ = data;
  buffer->data_length_ = data_length;
  buffer->allocator_ = allocator;
  return std::move(buffer);
}

Buffer::~Buffer() {
  if (!parent_) {
    iree_allocator_free(allocator_, data_);
    data_ = nullptr;
  }
  parent_.reset();
}

Status Buffer::MakeRange(iree_vmla_size_t byte_offset,
                         iree_vmla_size_t byte_length,
                         absl::Span<uint8_t>* out_range) const {
  if (byte_length == kVMLAWholeBuffer) {
    byte_length = size() - byte_offset;
  }
  if (byte_offset > size()) {
    return iree_make_status(
        IREE_STATUS_OUT_OF_RANGE,
        "attempted to access an address off the end of the valid buffer range "
        "(offset=%u, length=%u, byte_length=%zu)",
        byte_offset, byte_length, size());
  }
  size_t end = byte_offset + byte_length - 1;
  if (end >= size()) {
    return iree_make_status(
        IREE_STATUS_OUT_OF_RANGE,
        "attempted to access an address off the end of the valid buffer range "
        "(offset=%u, length=%u, end=%zu, byte_length=%zu)",
        byte_offset, byte_length, end, size());
  }
  uint8_t* data = reinterpret_cast<uint8_t*>(data_) + byte_offset;
  size_t data_length = byte_length;
  *out_range = absl::MakeSpan(data, data_length);
  return OkStatus();
}

constexpr int Interface::kMaxConstants;
constexpr int Interface::kMaxSets;
constexpr int Interface::kMaxBindings;

void Interface::Reset() {
  for (size_t i = 0; i < bindings_.size(); ++i) {
    for (size_t j = 0; j < bindings_[i].size(); ++j) {
      bindings_[i][j] = {};
    }
  }
}

StatusOr<uint32_t> Interface::GetConstant(uint32_t offset) const {
  if (offset >= kMaxConstants) {
    return iree_make_status(IREE_STATUS_INVALID_ARGUMENT,
                            "invalid constant offset=%u", offset);
  }
  return constants_[offset];
}

Status Interface::SetConstants(absl::Span<const uint32_t> values) {
  if (values.size() > kMaxConstants) {
    return iree_make_status(IREE_STATUS_INVALID_ARGUMENT,
                            "constant value overflow; have %zu but max is %d",
                            values.size(), kMaxConstants);
  }
  for (size_t i = 0; i < values.size(); ++i) {
    constants_[i] = values[i];
  }
  return OkStatus();
}

StatusOr<const Interface::Binding> Interface::GetBinding(
    uint32_t set, uint32_t binding) const {
  if (set >= kMaxSets || binding >= kMaxBindings) {
    return iree_make_status(IREE_STATUS_INVALID_ARGUMENT,
                            "invalid binding set=%u, binding=%u", set, binding);
  }
  return bindings_[set][binding];
}

Status Interface::SetBinding(uint32_t set, uint32_t binding, Binding value) {
  if (set >= kMaxSets || binding >= kMaxBindings) {
    return iree_make_status(IREE_STATUS_INVALID_ARGUMENT,
                            "invalid binding set=%u, binding=%u", set, binding);
  }
  bindings_[set][binding] = std::move(value);
  return OkStatus();
}

//===----------------------------------------------------------------------===//
// Module state and method implementation
//===----------------------------------------------------------------------===//

namespace {

// Per-executable VMLA module state.
// This provides the exported kernel functions to the VM and is instantiated
// one or more times per executable used within a device. Any state here can be
// treated as workgroup-local memory.
//
// Thread-compatible.
class VMLAModuleState final {
 public:
  VMLAModuleState(iree_allocator_t allocator,
                  kernels::RuntimeState* kernel_state)
      : allocator_(allocator), kernel_state_(kernel_state) {}

  ~VMLAModuleState() = default;

  //===--------------------------------------------------------------------===//
  // vmla.interface.*
  //===--------------------------------------------------------------------===//

  StatusOr<uint32_t> InterfaceConst(vm::ref<Interface> interface,
                                    uint32_t offset) {
    IREE_TRACE_SCOPE0("VMLAModuleState::InterfaceConst");
    return interface->GetConstant(offset);
  }

  StatusOr<vm::ref<Buffer>> InterfaceBinding(vm::ref<Interface> interface,
                                             uint32_t set, uint32_t binding) {
    IREE_TRACE_SCOPE0("VMLAModuleState::InterfaceBinding");
    IREE_ASSIGN_OR_RETURN(const auto& value,
                          interface->GetBinding(set, binding));
    return vm::retain_ref(value.buffer);
  }

  //===--------------------------------------------------------------------===//
  // vmla.buffer.*
  //===--------------------------------------------------------------------===//

  StatusOr<vm::ref<Buffer>> BufferConst(
      vm::ref<iree_vm_ro_byte_buffer_t> value) {
    IREE_TRACE_SCOPE0("VMLAModuleState::BufferConst");
    iree_allocator_t external_allocator = {0};
    external_allocator.self = vm::retain_ref(value).release();
    external_allocator.free = +[](void* self, void* ptr) {
      vm::assign_ref(reinterpret_cast<iree_vm_ro_byte_buffer_t*>(self)).reset();
    };
    return Buffer::Wrap(value->data.data, value->data.data_length,
                        external_allocator);
  }

  StatusOr<vm::ref<Buffer>> BufferAlloc(iree_vmla_size_t byte_length) {
    IREE_TRACE_SCOPE0("VMLAModuleState::BufferAlloc");
    return Buffer::Allocate(byte_length, allocator_);
  }

  StatusOr<vm::ref<Buffer>> BufferClone(const vm::ref<Buffer>& src) {
    IREE_TRACE_SCOPE0("VMLAModuleState::BufferClone");
    IREE_ASSIGN_OR_RETURN(auto dst, Buffer::Allocate(src->size(), allocator_));
    std::memcpy(dst->data(), src->data(), dst->size());
    return std::move(dst);
  }

  StatusOr<iree_vmla_size_t> BufferByteLength(const vm::ref<Buffer>& buffer) {
    IREE_TRACE_SCOPE0("VMLAModuleState::BufferByteLength");
    return buffer->size();
  }

  StatusOr<vm::ref<Buffer>> BufferView(const vm::ref<Buffer>& src,
                                       iree_vmla_size_t byte_offset,
                                       iree_vmla_size_t byte_length) {
    IREE_TRACE_SCOPE0("VMLAModuleState::BufferView");

    if (byte_length == kVMLAWholeBuffer) {
      byte_length = src->size() - byte_offset;
    }

    if (byte_offset == 0 && byte_length == src->size()) {
      // Asking for the same buffer.
      return vm::retain_ref(src);
    } else if (byte_offset > src->size()) {
      return iree_make_status(IREE_STATUS_OUT_OF_RANGE,
                              "attempted to access an address off the end of "
                              "the valid buffer range "
                              "(offset=%u, length=%u, byte_length=%zu)",
                              byte_offset, byte_length, src->size());
    }
    size_t end = byte_offset + byte_length - 1;
    if (end >= src->size()) {
      return iree_make_status(
          IREE_STATUS_OUT_OF_RANGE,
          "attempted to access an address off the end of the valid buffer "
          "range (offset=%u, length=%u, end=%zu, byte_length=%zu)",
          byte_offset, byte_length, end, src->size());
    }
    uint8_t* data = reinterpret_cast<uint8_t*>(src->data()) + byte_offset;
    size_t data_length = byte_length;

    iree_allocator_t external_allocator = {0};
    external_allocator.self = vm::retain_ref(src).release();
    external_allocator.free = +[](void* self, void* ptr) {
      vm::assign_ref(reinterpret_cast<Buffer*>(self)).reset();
    };
    return Buffer::Wrap(data, data_length, external_allocator);
  }

  Status BufferCopy(const vm::ref<Buffer>& src,
                    iree_vmla_size_t src_byte_offset,
                    const vm::ref<Buffer>& dst,
                    iree_vmla_size_t dst_byte_offset,
                    iree_vmla_size_t byte_length) {
    IREE_TRACE_SCOPE0("VMLAModuleState::BufferCopy");
    if (byte_length == kVMLAWholeBuffer) {
      byte_length = src->size() - src_byte_offset;
    }
    IREE_ASSIGN_OR_RETURN(auto src_bytes, src->RangeAs<const uint8_t>(
                                              src_byte_offset, byte_length));
    IREE_ASSIGN_OR_RETURN(auto dst_bytes,
                          dst->RangeAs<uint8_t>(dst_byte_offset, byte_length));
    std::memcpy(dst_bytes.data(), src_bytes.data(), dst_bytes.size());
    return OkStatus();
  }

  Status BufferFill(const vm::ref<Buffer>& value, const vm::ref<Buffer>& dst) {
    IREE_TRACE_SCOPE0("VMLAModuleState::BufferFill");
    if (value->size() == 1) {
      // Fast-path for single-byte memset values.
      std::memset(dst->data(), value->As<uint8_t>()[0], dst->size());
      return OkStatus();
    } else if (dst->size() % value->size() != 0) {
      return iree_make_status(
          IREE_STATUS_INVALID_ARGUMENT,
          "fill value length (%zu) must divide evenly into buffer length (%zu)",
          value->size(), dst->size());
    }
    auto value_bytes = value->As<uint8_t>();
    auto dst_bytes = dst->As<uint8_t>();
    for (size_t i = 0; i < dst_bytes.size(); i += value_bytes.size()) {
      std::memcpy(dst_bytes.data() + i, value_bytes.data(), value_bytes.size());
    }
    return OkStatus();
  }

  StatusOr<int32_t> BufferLoadI32(const vm::ref<Buffer>& src,
                                  iree_vmla_size_t byte_offset) {
    IREE_TRACE_SCOPE0("VMLAModuleState::BufferLoadI32");
    IREE_ASSIGN_OR_RETURN(auto data,
                          src->RangeAs<int32_t>(byte_offset, sizeof(int32_t)));
    return data[0];
  }

  //===--------------------------------------------------------------------===//
  // Common helpers for defining ops
  //===--------------------------------------------------------------------===//

#define IREE_VMLA_NONARY_OP(name, kernel, type)    \
  Status name(const vm::ref<Buffer>& dst) {        \
    IREE_TRACE_SCOPE0("VMLAModuleState::" #name);  \
    return kernel::Execute<type>(dst->As<type>()); \
  }

#define IREE_VMLA_UNARY_OP(name, kernel, type)                          \
  Status name(const vm::ref<Buffer>& src, const vm::ref<Buffer>& dst) { \
    IREE_TRACE_SCOPE0("VMLAModuleState::" #name);                       \
    return kernel::Execute<type>(src->As<type>(), dst->As<type>());     \
  }

#define IREE_VMLA_BINARY_OP(name, kernel, type)                       \
  Status name(const vm::ref<Buffer>& lhs, const vm::ref<Buffer>& rhs, \
              const vm::ref<Buffer>& dst) {                           \
    IREE_TRACE_SCOPE0("VMLAModuleState::" #name);                     \
    return kernel::Execute<type>(lhs->As<type>(), rhs->As<type>(),    \
                                 dst->As<type>());                    \
  }

#define IREE_VMLA_BINARY_BROADCAST_OP(name, kernel, type)                 \
  Status name(const vm::ref<Buffer>& lhs, int32_t rhs,                    \
              const vm::ref<Buffer>& dst) {                               \
    IREE_TRACE_SCOPE0("VMLAModuleState::" #name);                         \
    return kernel::Execute<type>(lhs->As<type>(), static_cast<type>(rhs), \
                                 dst->As<type>());                        \
  }

#define IREE_VMLA_TERNARY_OP(name, kernel, type)                              \
  Status name(const vm::ref<Buffer>& a, const vm::ref<Buffer>& b,             \
              const vm::ref<Buffer>& c, const vm::ref<Buffer>& dst) {         \
    IREE_TRACE_SCOPE0("VMLAModuleState::" #name);                             \
    return kernel::Execute<type>(a->As<type>(), b->As<type>(), c->As<type>(), \
                                 dst->As<type>());                            \
  }

  //===--------------------------------------------------------------------===//
  // VMLA Ops: comparison
  //===--------------------------------------------------------------------===//

  enum class CmpPredicate : uint32_t {
    kEQ = 0,
    kNE = 1,
    kLT = 2,
    kLE = 3,
    kGT = 4,
    kGE = 5,
  };

#define IREE_VMLA_COMPARE_OP(name, type)                                \
  Status name(int32_t predicate, const vm::ref<Buffer>& lhs,            \
              const vm::ref<Buffer>& rhs, const vm::ref<Buffer>& dst) { \
    IREE_TRACE_SCOPE0("VMLAModuleState::" #name);                       \
    switch (static_cast<CmpPredicate>(predicate)) {                     \
      case CmpPredicate::kEQ:                                           \
        return kernels::CompareEQ::Execute<type>(                       \
            lhs->As<type>(), rhs->As<type>(), dst->As<uint8_t>());      \
      case CmpPredicate::kNE:                                           \
        return kernels::CompareNE::Execute<type>(                       \
            lhs->As<type>(), rhs->As<type>(), dst->As<uint8_t>());      \
      case CmpPredicate::kLT:                                           \
        return kernels::CompareLT::Execute<type>(                       \
            lhs->As<type>(), rhs->As<type>(), dst->As<uint8_t>());      \
      case CmpPredicate::kLE:                                           \
        return kernels::CompareLE::Execute<type>(                       \
            lhs->As<type>(), rhs->As<type>(), dst->As<uint8_t>());      \
      case CmpPredicate::kGT:                                           \
        return kernels::CompareGT::Execute<type>(                       \
            lhs->As<type>(), rhs->As<type>(), dst->As<uint8_t>());      \
      case CmpPredicate::kGE:                                           \
        return kernels::CompareGE::Execute<type>(                       \
            lhs->As<type>(), rhs->As<type>(), dst->As<uint8_t>());      \
      default:                                                          \
        return iree_make_status(IREE_STATUS_INVALID_ARGUMENT,           \
                                "unsupported predicate %d", predicate); \
    }                                                                   \
  }
  IREE_VMLA_COMPARE_OP(CmpI8, int8_t);
  IREE_VMLA_COMPARE_OP(CmpI16, int16_t);
  IREE_VMLA_COMPARE_OP(CmpI32, int32_t);
  IREE_VMLA_COMPARE_OP(CmpF32, float);

#define IREE_VMLA_SELECT_OP(name, type)                                     \
  Status name(const vm::ref<Buffer>& cond, const vm::ref<Buffer>& lhs,      \
              const vm::ref<Buffer>& rhs, const vm::ref<Buffer>& dst) {     \
    IREE_TRACE_SCOPE0("VMLAModuleState::" #name);                           \
    return kernels::Select::Execute<type>(cond->As<uint8_t>(),              \
                                          lhs->As<type>(), rhs->As<type>(), \
                                          dst->As<type>());                 \
  }
  IREE_VMLA_SELECT_OP(SelectX8, uint8_t);
  IREE_VMLA_SELECT_OP(SelectX16, uint16_t);
  IREE_VMLA_SELECT_OP(SelectX32, uint32_t);

#define IREE_VMLA_UNARY_PREDICATE_OP(name, kernel, type)                \
  Status name(const vm::ref<Buffer>& src, const vm::ref<Buffer>& dst) { \
    IREE_TRACE_SCOPE0("VMLAModuleState::" #name);                       \
    return kernel::Execute<type>(src->As<type>(), dst->As<bool>());     \
  }
  IREE_VMLA_UNARY_PREDICATE_OP(FiniteF32, kernels::Finite, float);

  //===--------------------------------------------------------------------===//
  // VMLA Ops: shape/structure
  //===--------------------------------------------------------------------===//

#define IREE_VMLA_COPY_OP(name, size)                                     \
  Status name(const vm::ref<Buffer>& src, iree_vmla_shape_t src_shape,    \
              absl::Span<const int32_t> src_indices,                      \
              const vm::ref<Buffer>& dst, iree_vmla_shape_t dst_shape,    \
              absl::Span<const int32_t> dst_indices,                      \
              absl::Span<const int32_t> lengths) {                        \
    IREE_TRACE_SCOPE0("VMLAModuleState::" #name);                         \
    return kernels::Copy::Execute<size>(src->As<uint8_t>(), src_shape,    \
                                        src_indices, dst->As<uint8_t>(),  \
                                        dst_shape, dst_indices, lengths); \
  }
  IREE_VMLA_COPY_OP(CopyX8, sizeof(uint8_t));
  IREE_VMLA_COPY_OP(CopyX16, sizeof(uint16_t));
  IREE_VMLA_COPY_OP(CopyX32, sizeof(uint32_t));

#define IREE_VMLA_TRANSPOSE_OP(name, type)                                     \
  Status name(const vm::ref<Buffer>& src, iree_vmla_shape_t src_shape,         \
              absl::Span<const int32_t> permutation,                           \
              const vm::ref<Buffer>& dst, iree_vmla_shape_t dst_shape) {       \
    IREE_TRACE_SCOPE0("VMLAModuleState::" #name);                              \
    return kernels::Transpose::Execute<type>(src->As<type>(), dst->As<type>(), \
                                             src_shape, permutation);          \
  }
  IREE_VMLA_TRANSPOSE_OP(TransposeX8, uint8_t);
  IREE_VMLA_TRANSPOSE_OP(TransposeX16, uint16_t);
  IREE_VMLA_TRANSPOSE_OP(TransposeX32, uint32_t);

#define IREE_VMLA_REVERSE_OP(name, type)                                     \
  Status name(const vm::ref<Buffer>& src, iree_vmla_shape_t src_shape,       \
              absl::Span<const int32_t> dims, const vm::ref<Buffer>& dst,    \
              iree_vmla_shape_t dst_shape) {                                 \
    IREE_TRACE_SCOPE0("VMLAModuleState::" #name);                            \
    return kernels::Reverse::Execute<type>(src->As<type>(), dst->As<type>(), \
                                           src_shape, dims);                 \
  }
  IREE_VMLA_REVERSE_OP(ReverseX8, uint8_t);
  IREE_VMLA_REVERSE_OP(ReverseX16, uint16_t);
  IREE_VMLA_REVERSE_OP(ReverseX32, uint32_t);

#define IREE_VMLA_PAD_OP(name, type)                                       \
  Status name(const vm::ref<Buffer>& src, iree_vmla_shape_t src_shape,     \
              const vm::ref<Buffer>& value, iree_vmla_shape_t value_shape, \
              const vm::ref<Buffer>& dst, iree_vmla_shape_t dst_shape,     \
              absl::Span<const int32_t> edge_padding_low,                  \
              absl::Span<const int32_t> edge_padding_high,                 \
              absl::Span<const int32_t> interior_padding) {                \
    IREE_TRACE_SCOPE0("VMLAModuleState::" #name);                          \
    return kernels::Pad::Execute<type>(                                    \
        src->As<type>(), value->As<type>(), dst->As<type>(), src_shape,    \
        dst_shape, edge_padding_low, edge_padding_high, interior_padding); \
  }
  IREE_VMLA_PAD_OP(PadX8, uint8_t);
  IREE_VMLA_PAD_OP(PadX16, uint16_t);
  IREE_VMLA_PAD_OP(PadX32, uint32_t);

#define IREE_VMLA_GATHER_OP(name, type)                                        \
  Status name(const vm::ref<Buffer>& src, iree_vmla_shape_t src_shape,         \
              const vm::ref<Buffer>& indices, iree_vmla_shape_t indices_shape, \
              const vm::ref<Buffer>& dst, iree_vmla_shape_t dst_shape,         \
              const int32_t dim, const int32_t batch_dims) {                   \
    IREE_TRACE_SCOPE0("VMLAModuleState::" #name);                              \
    return kernels::Gather::Execute<type>(                                     \
        src->As<type>(), indices->As<int>(), dst->As<type>(), src_shape,       \
        indices_shape, dst_shape, dim, batch_dims);                            \
  }
  IREE_VMLA_GATHER_OP(GatherX8, uint8_t);
  IREE_VMLA_GATHER_OP(GatherX16, uint16_t);
  IREE_VMLA_GATHER_OP(GatherX32, uint32_t);

#define IREE_VMLA_SCATTER_OP(name, type)                                       \
  Status name(const vm::ref<Buffer>& src, iree_vmla_shape_t src_shape,         \
              const vm::ref<Buffer>& indices, iree_vmla_shape_t indices_shape, \
              const vm::ref<Buffer>& dst, iree_vmla_shape_t dst_shape) {       \
    IREE_TRACE_SCOPE0("VMLAModuleState::" #name);                              \
    return kernels::Scatter::Execute<type>(                                    \
        src->As<type>(), indices->As<int>(), dst->As<type>(), src_shape,       \
        indices_shape, dst_shape);                                             \
  }
  IREE_VMLA_SCATTER_OP(ScatterX8, uint8_t);
  IREE_VMLA_SCATTER_OP(ScatterX16, uint16_t);
  IREE_VMLA_SCATTER_OP(ScatterX32, uint32_t);

#define IREE_VMLA_BROADCAST_OP(name, type)                               \
  Status name(const vm::ref<Buffer>& src, iree_vmla_shape_t src_shape,   \
              const vm::ref<Buffer>& dst, iree_vmla_shape_t dst_shape) { \
    IREE_TRACE_SCOPE0("VMLAModuleState::" #name);                        \
    return kernels::Broadcast::Execute<type>(src->As<type>(),            \
                                             dst->As<type>());           \
  }
  IREE_VMLA_BROADCAST_OP(BroadcastX8, uint8_t);
  IREE_VMLA_BROADCAST_OP(BroadcastX16, uint16_t);
  IREE_VMLA_BROADCAST_OP(BroadcastX32, uint32_t);

  IREE_VMLA_NONARY_OP(IotaI8, kernels::Iota, int8_t);
  IREE_VMLA_NONARY_OP(IotaI16, kernels::Iota, int16_t);
  IREE_VMLA_NONARY_OP(IotaI32, kernels::Iota, int32_t);
  IREE_VMLA_NONARY_OP(IotaF32, kernels::Iota, float_t);

#define IREE_VMLA_TILE_OP(name, type)                                     \
  Status name(const vm::ref<Buffer>& src, iree_vmla_shape_t src_shape,    \
              const vm::ref<Buffer>& dst, iree_vmla_shape_t dst_shape) {  \
    IREE_TRACE_SCOPE0("VMLAModuleState::" #name);                         \
    return kernels::Tile::Execute<type>(src->As<type>(), dst->As<type>(), \
                                        src_shape, dst_shape);            \
  }
  IREE_VMLA_TILE_OP(TileX8, uint8_t);
  IREE_VMLA_TILE_OP(TileX16, uint16_t);
  IREE_VMLA_TILE_OP(TileX32, uint32_t);

  //===--------------------------------------------------------------------===//
  // VMLA Ops: bit manipulation
  //===--------------------------------------------------------------------===//

  IREE_VMLA_UNARY_OP(NotX8, kernels::Not, uint8_t);
  IREE_VMLA_UNARY_OP(NotX16, kernels::Not, uint16_t);
  IREE_VMLA_UNARY_OP(NotX32, kernels::Not, uint32_t);
  IREE_VMLA_BINARY_OP(AndX8, kernels::And, uint8_t);
  IREE_VMLA_BINARY_OP(AndX16, kernels::And, uint16_t);
  IREE_VMLA_BINARY_OP(AndX32, kernels::And, uint32_t);
  IREE_VMLA_BINARY_BROADCAST_OP(AndBroadcastX8, kernels::And, uint8_t);
  IREE_VMLA_BINARY_BROADCAST_OP(AndBroadcastX16, kernels::And, uint16_t);
  IREE_VMLA_BINARY_BROADCAST_OP(AndBroadcastX32, kernels::And, uint32_t);
  IREE_VMLA_BINARY_OP(OrX8, kernels::Or, uint8_t);
  IREE_VMLA_BINARY_OP(OrX16, kernels::Or, uint16_t);
  IREE_VMLA_BINARY_OP(OrX32, kernels::Or, uint32_t);
  IREE_VMLA_BINARY_OP(XorX8, kernels::Xor, uint8_t);
  IREE_VMLA_BINARY_OP(XorX16, kernels::Xor, uint16_t);
  IREE_VMLA_BINARY_OP(XorX32, kernels::Xor, uint32_t);
  IREE_VMLA_BINARY_BROADCAST_OP(XorBroadcastX8, kernels::Xor, uint8_t);
  IREE_VMLA_BINARY_BROADCAST_OP(XorBroadcastX16, kernels::Xor, uint16_t);
  IREE_VMLA_BINARY_BROADCAST_OP(XorBroadcastX32, kernels::Xor, uint32_t);
  IREE_VMLA_BINARY_OP(ShlX8, kernels::ShiftLeft, uint8_t);
  IREE_VMLA_BINARY_OP(ShlX16, kernels::ShiftLeft, uint16_t);
  IREE_VMLA_BINARY_OP(ShlX32, kernels::ShiftLeft, uint32_t);
  IREE_VMLA_BINARY_OP(ShrU8, kernels::ShiftRight, uint8_t);
  IREE_VMLA_BINARY_OP(ShrU16, kernels::ShiftRight, uint16_t);
  IREE_VMLA_BINARY_OP(ShrU32, kernels::ShiftRight, uint32_t);
  IREE_VMLA_BINARY_OP(ShrI8, kernels::ShiftRight, int8_t);
  IREE_VMLA_BINARY_OP(ShrI16, kernels::ShiftRight, int16_t);
  IREE_VMLA_BINARY_OP(ShrI32, kernels::ShiftRight, int32_t);

  //===--------------------------------------------------------------------===//
  // VMLA Ops: arithmetic
  //===--------------------------------------------------------------------===//

  IREE_VMLA_BINARY_OP(AddI8, kernels::Add, int8_t);
  IREE_VMLA_BINARY_OP(AddI16, kernels::Add, int16_t);
  IREE_VMLA_BINARY_OP(AddI32, kernels::Add, int32_t);
  IREE_VMLA_BINARY_OP(AddF32, kernels::Add, float);
  IREE_VMLA_BINARY_OP(SubI8, kernels::Sub, int8_t);
  IREE_VMLA_BINARY_OP(SubI16, kernels::Sub, int16_t);
  IREE_VMLA_BINARY_OP(SubI32, kernels::Sub, int32_t);
  IREE_VMLA_BINARY_OP(SubF32, kernels::Sub, float);
  IREE_VMLA_UNARY_OP(AbsI8, kernels::Abs, int8_t);
  IREE_VMLA_UNARY_OP(AbsI16, kernels::Abs, int16_t);
  IREE_VMLA_UNARY_OP(AbsI32, kernels::Abs, int32_t);
  IREE_VMLA_UNARY_OP(AbsF32, kernels::Abs, float);
  IREE_VMLA_UNARY_OP(NegI8, kernels::Neg, int8_t);
  IREE_VMLA_UNARY_OP(NegI16, kernels::Neg, int16_t);
  IREE_VMLA_UNARY_OP(NegI32, kernels::Neg, int32_t);
  IREE_VMLA_UNARY_OP(NegF32, kernels::Neg, float);
  IREE_VMLA_BINARY_OP(MulI8, kernels::Mul, int8_t);
  IREE_VMLA_BINARY_OP(MulI16, kernels::Mul, int16_t);
  IREE_VMLA_BINARY_OP(MulI32, kernels::Mul, int32_t);
  IREE_VMLA_BINARY_OP(MulF32, kernels::Mul, float);
  IREE_VMLA_BINARY_OP(DivI8, kernels::Div, int8_t);
  IREE_VMLA_BINARY_OP(DivI16, kernels::Div, int16_t);
  IREE_VMLA_BINARY_OP(DivI32, kernels::Div, int32_t);
  IREE_VMLA_BINARY_OP(DivU8, kernels::Div, uint8_t);
  IREE_VMLA_BINARY_OP(DivU16, kernels::Div, uint16_t);
  IREE_VMLA_BINARY_OP(DivU32, kernels::Div, uint32_t);
  IREE_VMLA_BINARY_OP(DivF32, kernels::Div, float);
  IREE_VMLA_BINARY_OP(RemI8, kernels::Rem, int8_t);
  IREE_VMLA_BINARY_OP(RemI16, kernels::Rem, int16_t);
  IREE_VMLA_BINARY_OP(RemI32, kernels::Rem, int32_t);
  IREE_VMLA_BINARY_OP(RemU8, kernels::Rem, uint8_t);
  IREE_VMLA_BINARY_OP(RemU16, kernels::Rem, uint16_t);
  IREE_VMLA_BINARY_OP(RemU32, kernels::Rem, uint32_t);
  IREE_VMLA_BINARY_OP(RemF32, kernels::Rem, float);
  IREE_VMLA_BINARY_OP(PowF32, kernels::Pow, float);
  IREE_VMLA_UNARY_OP(ExpF32, kernels::Exp, float);
  IREE_VMLA_UNARY_OP(LogF32, kernels::Log, float);
  IREE_VMLA_UNARY_OP(RsqrtF32, kernels::Rsqrt, float);
  IREE_VMLA_UNARY_OP(SqrtF32, kernels::Sqrt, float);
  IREE_VMLA_UNARY_OP(CosF32, kernels::Cos, float);
  IREE_VMLA_UNARY_OP(SinF32, kernels::Sin, float);
  IREE_VMLA_UNARY_OP(TanhF32, kernels::Tanh, float);
  IREE_VMLA_BINARY_OP(Atan2F32, kernels::Atan2, float);

  IREE_VMLA_BINARY_OP(MinI8, kernels::Min, int8_t);
  IREE_VMLA_BINARY_OP(MinI16, kernels::Min, int16_t);
  IREE_VMLA_BINARY_OP(MinI32, kernels::Min, int32_t);
  IREE_VMLA_BINARY_OP(MinF32, kernels::Min, float);
  IREE_VMLA_BINARY_OP(MaxI8, kernels::Max, int8_t);
  IREE_VMLA_BINARY_OP(MaxI16, kernels::Max, int16_t);
  IREE_VMLA_BINARY_OP(MaxI32, kernels::Max, int32_t);
  IREE_VMLA_BINARY_OP(MaxF32, kernels::Max, float);
  IREE_VMLA_TERNARY_OP(ClampI8, kernels::Clamp, int8_t);
  IREE_VMLA_TERNARY_OP(ClampI16, kernels::Clamp, int16_t);
  IREE_VMLA_TERNARY_OP(ClampI32, kernels::Clamp, int32_t);
  IREE_VMLA_TERNARY_OP(ClampF32, kernels::Clamp, float);
  IREE_VMLA_UNARY_OP(FloorF32, kernels::Floor, float);
  IREE_VMLA_UNARY_OP(CeilF32, kernels::Ceil, float);
  IREE_VMLA_UNARY_OP(RoundF32, kernels::Round, float);

#define IREE_VMLA_SORT_OP(name, type)                                        \
  Status name(const vm::ref<Buffer>& src, iree_vmla_shape_t src_shape,       \
              const vm::ref<Buffer>& dst) {                                  \
    IREE_TRACE_SCOPE0("VMLAModuleState::" #name);                            \
    return kernels::Sort::Execute<type>(src->As<type>(), dst->As<int32_t>(), \
                                        src_shape);                          \
  }

  IREE_VMLA_SORT_OP(SortI8, int8_t);
  IREE_VMLA_SORT_OP(SortI16, int16_t);
  IREE_VMLA_SORT_OP(SortI32, int32_t);
  IREE_VMLA_SORT_OP(SortF32, float);

#define IREE_VMLA_COMPLEX_INPUT_FFT_OP(name, op)                            \
  Status name(                                                              \
      const vm::ref<Buffer>& real_src, iree_vmla_shape_t real_src_shape,    \
      const vm::ref<Buffer>& imag_src, iree_vmla_shape_t imag_src_shape,    \
      const vm::ref<Buffer>& real_dst, const vm::ref<Buffer>& imag_dst) {   \
    IREE_TRACE_SCOPE0("VMLAModuleState::" #name);                           \
    return op::Execute<float>(real_src->As<float>(), imag_src->As<float>(), \
                              real_dst->As<float>(), imag_dst->As<float>(), \
                              real_src_shape, imag_src_shape);              \
  }

  IREE_VMLA_COMPLEX_INPUT_FFT_OP(FftF32, kernels::Fft);
  IREE_VMLA_COMPLEX_INPUT_FFT_OP(IfftF32, kernels::Ifft);

  Status RfftF32(const vm::ref<Buffer>& real_src,
                 iree_vmla_shape_t real_src_shape,
                 const vm::ref<Buffer>& real_dst,
                 const vm::ref<Buffer>& imag_dst) {
    IREE_TRACE_SCOPE0("VMLAModuleState::RfftF32");
    IREE_RETURN_IF_ERROR(kernels::Rfft::Execute<float>(
        real_src->As<float>(), real_dst->As<float>(), imag_dst->As<float>(),
        real_src_shape));
    return OkStatus();
  }

  Status IrfftF32(const vm::ref<Buffer>& real_src,
                  iree_vmla_shape_t real_src_shape,
                  const vm::ref<Buffer>& imag_src,
                  iree_vmla_shape_t imag_src_shape,
                  const vm::ref<Buffer>& real_dst) {
    IREE_TRACE_SCOPE0("VMLAModuleState::IrfftF32");
    IREE_RETURN_IF_ERROR(kernels::Irfft::Execute<float>(
        real_src->As<float>(), imag_src->As<float>(), real_dst->As<float>(),
        real_src_shape, imag_src_shape));
    return OkStatus();
  }

  //===--------------------------------------------------------------------===//
  // VMLA Ops: conversion
  //===--------------------------------------------------------------------===//

#define IREE_VMLA_CONVERSION_OP(name, src_type, dst_type)                      \
  Status name(const vm::ref<Buffer>& src, const vm::ref<Buffer>& dst) {        \
    IREE_TRACE_SCOPE0("VMLAModuleState::" #name);                              \
    return kernels::Convert::Execute<src_type, dst_type>(src->As<src_type>(),  \
                                                         dst->As<dst_type>()); \
  }
  IREE_VMLA_CONVERSION_OP(ConvertI8I16, int8_t, int16_t);
  IREE_VMLA_CONVERSION_OP(ConvertI8I32, int8_t, int32_t);
  IREE_VMLA_CONVERSION_OP(ConvertI8F32, int8_t, float);
  IREE_VMLA_CONVERSION_OP(ConvertI16I8, int16_t, int8_t);
  IREE_VMLA_CONVERSION_OP(ConvertI16I32, int16_t, int32_t);
  IREE_VMLA_CONVERSION_OP(ConvertI16F32, int16_t, float);
  IREE_VMLA_CONVERSION_OP(ConvertI32I8, int32_t, int8_t);
  IREE_VMLA_CONVERSION_OP(ConvertI32I16, int32_t, int16_t);
  IREE_VMLA_CONVERSION_OP(ConvertI32F32, int32_t, float);
  IREE_VMLA_CONVERSION_OP(ConvertF32I8, float, int8_t);
  IREE_VMLA_CONVERSION_OP(ConvertF32I16, float, int16_t);
  IREE_VMLA_CONVERSION_OP(ConvertF32I32, float, int32_t);

  //===--------------------------------------------------------------------===//
  // VMLA Ops: Convolution
  //===--------------------------------------------------------------------===//

  Status ConvF32F32F32(
      const vm::ref<Buffer>& input, iree_vmla_shape_t input_shape,
      const vm::ref<Buffer>& filter, iree_vmla_shape_t filter_shape,
      const vm::ref<Buffer>& dst, iree_vmla_shape_t dst_shape,
      absl::Span<const int32_t> window_strides,
      absl::Span<const int32_t> padding, absl::Span<const int32_t> lhs_dilation,
      absl::Span<const int32_t> rhs_dilation, const int32_t feature_group_count,
      const int32_t batch_group_count) {
    IREE_TRACE_SCOPE0("VMLAModuleState::ConvF32F32F32");
    if (input_shape.size() != 4 || filter_shape.size() != 4 ||
        dst_shape.size() != 4) {
      return iree_make_status(IREE_STATUS_INVALID_ARGUMENT,
                              "expecting 4-d tensors for Conv2D kernel");
    }

    const int32_t batch_size = input_shape[0];
    const auto input_example_shape = input_shape.subspan(1, 3);
    const auto output_example_shape = dst_shape.subspan(1, 3);
    const auto filter_shape_4d = filter_shape.subspan(0, 4);
    const auto pad_h = padding.subspan(0, 2);
    const auto pad_w = padding.subspan(2, 2);
    const auto window_strides_2d = window_strides.subspan(0, 2);

    const float* raw_inputs_data = input->As<float>().data();
    const float* raw_filter_data = filter->As<float>().data();
    float* raw_dst_data = dst->As<float>().data();
    auto filter_buffer = absl::MakeConstSpan(
        raw_filter_data, kernels::GetElementCount(filter_shape_4d));

    const size_t input_stride = kernels::GetElementCount(input_example_shape);
    const size_t output_stride = kernels::GetElementCount(output_example_shape);

    for (int i = 0; i < batch_size; ++i) {
      auto input_example =
          absl::MakeConstSpan(raw_inputs_data + i * input_stride, input_stride);
      auto output_example =
          absl::MakeSpan(raw_dst_data + i * output_stride, output_stride);
      IREE_RETURN_IF_ERROR(kernels::Conv2D::Execute(
          input_example, input_example_shape, filter_buffer, filter_shape_4d,
          output_example, output_example_shape, window_strides_2d, pad_h, pad_w,
          lhs_dilation.subspan(0, 2), rhs_dilation.subspan(0, 2),
          feature_group_count));
    }
    return OkStatus();
  }

  //===--------------------------------------------------------------------===//
  // VMLA Ops: GEMM/GEMV
  //===--------------------------------------------------------------------===//

  template <typename LhsEl, typename RhsEl, typename AccumEl, typename DstEl>
  Status BatchMatMul(const vm::ref<Buffer>& lhs, iree_vmla_shape_t lhs_shape,
                     const vm::ref<Buffer>& rhs, iree_vmla_shape_t rhs_shape,
                     const vm::ref<Buffer>& dst, iree_vmla_shape_t dst_shape) {
    IREE_TRACE_SCOPE0("VMLAModuleState::BatchMatMul");
    // Compiler guarantees. Here for documentation purposes.
    assert(lhs_shape.size() == 3 && rhs_shape.size() == 3 &&
           dst_shape.size() == 3);
    assert(lhs_shape[0] == rhs_shape[0] && rhs_shape[0] == dst_shape[0]);

    iree_vmla_shape_t lhs_batch_element_shape = lhs_shape.subspan(1);
    iree_vmla_shape_t rhs_batch_element_shape = rhs_shape.subspan(1);
    iree_vmla_shape_t dst_batch_element_shape = dst_shape.subspan(1);
    auto lhs_batch_element_shape2 = lhs_batch_element_shape.subspan(0, 2);
    auto rhs_batch_element_shape2 = rhs_batch_element_shape.subspan(0, 2);
    auto dst_batch_element_shape2 = dst_batch_element_shape.subspan(0, 2);
    size_t lhs_batch_stride = kernels::GetElementCount(lhs_batch_element_shape);
    size_t rhs_batch_stride = kernels::GetElementCount(rhs_batch_element_shape);
    size_t dst_batch_stride = kernels::GetElementCount(dst_batch_element_shape);
    LhsEl* lhs_batch_base = lhs->As<LhsEl>().data();
    RhsEl* rhs_batch_base = rhs->As<RhsEl>().data();
    DstEl* dst_batch_base = dst->As<DstEl>().data();
    int32_t batch_dim = lhs_shape[0];
    for (int i = 0; i < batch_dim; i++) {
      kernels::MatMul::Buffers<LhsEl, RhsEl, AccumEl, DstEl> buffers;
      buffers.lhs_buffer = absl::MakeSpan(lhs_batch_base + i * lhs_batch_stride,
                                          lhs_batch_stride);
      buffers.lhs_shape = lhs_batch_element_shape2;
      buffers.rhs_buffer = absl::MakeSpan(rhs_batch_base + i * rhs_batch_stride,
                                          rhs_batch_stride);
      buffers.rhs_shape = rhs_batch_element_shape2;
      buffers.dst_buffer = absl::MakeSpan(dst_batch_base + i * dst_batch_stride,
                                          dst_batch_stride);
      buffers.dst_shape = dst_batch_element_shape2;

      IREE_RETURN_IF_ERROR(kernels::MatMul::Execute(
          kernel_state_->mat_mul_state.get(), buffers));
    }
    return OkStatus();
  }

  //===--------------------------------------------------------------------===//
  // VMLA Ops: reduction
  //===--------------------------------------------------------------------===//

#define IREE_VMLA_REDUCTION_OP(name, kernel, type)                       \
  Status name(const vm::ref<Buffer>& src, iree_vmla_shape_t src_shape,   \
              const vm::ref<Buffer>& init, iree_vmla_shape_t init_shape, \
              int32_t dimension, const vm::ref<Buffer>& dst,             \
              iree_vmla_shape_t dst_shape) {                             \
    IREE_TRACE_SCOPE0("VMLAModuleState::" #name);                        \
    return kernel::Execute<type>(src->As<type>(), init->As<type>(),      \
                                 dst->As<type>(), dimension, src_shape,  \
                                 dst_shape);                             \
  }
  IREE_VMLA_REDUCTION_OP(ReduceSumI8, kernels::ReduceSum, int8_t);
  IREE_VMLA_REDUCTION_OP(ReduceSumI16, kernels::ReduceSum, int16_t);
  IREE_VMLA_REDUCTION_OP(ReduceSumI32, kernels::ReduceSum, int32_t);
  IREE_VMLA_REDUCTION_OP(ReduceSumF32, kernels::ReduceSum, float);
  IREE_VMLA_REDUCTION_OP(ReduceMinI8, kernels::ReduceMin, int8_t);
  IREE_VMLA_REDUCTION_OP(ReduceMinI16, kernels::ReduceMin, int16_t);
  IREE_VMLA_REDUCTION_OP(ReduceMinI32, kernels::ReduceMin, int32_t);
  IREE_VMLA_REDUCTION_OP(ReduceMinF32, kernels::ReduceMin, float);
  IREE_VMLA_REDUCTION_OP(ReduceMaxI8, kernels::ReduceMax, int8_t);
  IREE_VMLA_REDUCTION_OP(ReduceMaxI16, kernels::ReduceMax, int16_t);
  IREE_VMLA_REDUCTION_OP(ReduceMaxI32, kernels::ReduceMax, int32_t);
  IREE_VMLA_REDUCTION_OP(ReduceMaxF32, kernels::ReduceMax, float);
  IREE_VMLA_REDUCTION_OP(ReduceAndI8, kernels::ReduceAnd, int8_t);
  IREE_VMLA_REDUCTION_OP(ReduceOrI8, kernels::ReduceOr, int8_t);

#define IREE_VMLA_POOLING_OP(name, kernel, type)                              \
  Status name(const vm::ref<Buffer>& src, iree_vmla_shape_t src_shape,        \
              const vm::ref<Buffer>& init, iree_vmla_shape_t init_shape,      \
              const vm::ref<Buffer>& dst, iree_vmla_shape_t dst_shape,        \
              iree_vmla_shape_t window_dimensions, iree_vmla_shape_t strides, \
              iree_vmla_shape_t pad_low) {                                    \
    IREE_TRACE_SCOPE0("VMLAModuleState::" #name);                             \
    return kernel::Execute<type>(src->As<type>(), init->As<type>(),           \
                                 dst->As<type>(), src_shape, dst_shape,       \
                                 window_dimensions, strides, pad_low);        \
  }
  IREE_VMLA_POOLING_OP(PoolingSumI8, kernels::PoolingSum, int8_t);
  IREE_VMLA_POOLING_OP(PoolingSumI16, kernels::PoolingSum, int16_t);
  IREE_VMLA_POOLING_OP(PoolingSumI32, kernels::PoolingSum, int32_t);
  IREE_VMLA_POOLING_OP(PoolingSumF32, kernels::PoolingSum, float);
  IREE_VMLA_POOLING_OP(PoolingMinI8, kernels::PoolingMin, int8_t);
  IREE_VMLA_POOLING_OP(PoolingMinI16, kernels::PoolingMin, int16_t);
  IREE_VMLA_POOLING_OP(PoolingMinI32, kernels::PoolingMin, int32_t);
  IREE_VMLA_POOLING_OP(PoolingMinF32, kernels::PoolingMin, float);
  IREE_VMLA_POOLING_OP(PoolingMaxI8, kernels::PoolingMax, int8_t);
  IREE_VMLA_POOLING_OP(PoolingMaxI16, kernels::PoolingMax, int16_t);
  IREE_VMLA_POOLING_OP(PoolingMaxI32, kernels::PoolingMax, int32_t);
  IREE_VMLA_POOLING_OP(PoolingMaxF32, kernels::PoolingMax, float);

 private:
  iree_allocator_t allocator_;

  // NOTE: kernel state must be externally synchronized as it is shared across
  // all contexts using the VMLA module. This is fine in our current design as
  // we only ever execute a single context at a time but if we start to allow
  // concurrency across contexts we'll need to introduce locks.
  kernels::RuntimeState* kernel_state_ = nullptr;
};

//===----------------------------------------------------------------------===//
// VM module interface implementation
//===----------------------------------------------------------------------===//

static const vm::NativeFunction<VMLAModuleState> kVMLAModuleFunctions[] = {
    vm::MakeNativeFunction("interface.const", &VMLAModuleState::InterfaceConst),
    vm::MakeNativeFunction("interface.binding",
                           &VMLAModuleState::InterfaceBinding),

    vm::MakeNativeFunction("buffer.const", &VMLAModuleState::BufferConst),
    vm::MakeNativeFunction("buffer.alloc", &VMLAModuleState::BufferAlloc),
    vm::MakeNativeFunction("buffer.clone", &VMLAModuleState::BufferClone),
    vm::MakeNativeFunction("buffer.view", &VMLAModuleState::BufferView),
    vm::MakeNativeFunction("buffer.copy", &VMLAModuleState::BufferCopy),
    vm::MakeNativeFunction("buffer.fill", &VMLAModuleState::BufferFill),
    vm::MakeNativeFunction("buffer.load.i32", &VMLAModuleState::BufferLoadI32),

    vm::MakeNativeFunction("cmp.i8", &VMLAModuleState::CmpI8),
    vm::MakeNativeFunction("cmp.i16", &VMLAModuleState::CmpI16),
    vm::MakeNativeFunction("cmp.i32", &VMLAModuleState::CmpI32),
    vm::MakeNativeFunction("cmp.f32", &VMLAModuleState::CmpF32),
    vm::MakeNativeFunction("select.x8", &VMLAModuleState::SelectX8),
    vm::MakeNativeFunction("select.x16", &VMLAModuleState::SelectX16),
    vm::MakeNativeFunction("select.x32", &VMLAModuleState::SelectX32),

    vm::MakeNativeFunction("broadcast.x8", &VMLAModuleState::BroadcastX8),
    vm::MakeNativeFunction("broadcast.x16", &VMLAModuleState::BroadcastX16),
    vm::MakeNativeFunction("broadcast.x32", &VMLAModuleState::BroadcastX32),
    vm::MakeNativeFunction("copy.x8", &VMLAModuleState::CopyX8),
    vm::MakeNativeFunction("copy.x16", &VMLAModuleState::CopyX16),
    vm::MakeNativeFunction("copy.x32", &VMLAModuleState::CopyX32),
    vm::MakeNativeFunction("transpose.x8", &VMLAModuleState::TransposeX8),
    vm::MakeNativeFunction("transpose.x16", &VMLAModuleState::TransposeX16),
    vm::MakeNativeFunction("transpose.x32", &VMLAModuleState::TransposeX32),
    vm::MakeNativeFunction("reverse.x8", &VMLAModuleState::ReverseX8),
    vm::MakeNativeFunction("reverse.x16", &VMLAModuleState::ReverseX16),
    vm::MakeNativeFunction("reverse.x32", &VMLAModuleState::ReverseX32),
    vm::MakeNativeFunction("pad.x8", &VMLAModuleState::PadX8),
    vm::MakeNativeFunction("pad.x16", &VMLAModuleState::PadX16),
    vm::MakeNativeFunction("pad.x32", &VMLAModuleState::PadX32),
    vm::MakeNativeFunction("gather.x8", &VMLAModuleState::GatherX8),
    vm::MakeNativeFunction("gather.x16", &VMLAModuleState::GatherX16),
    vm::MakeNativeFunction("gather.x32", &VMLAModuleState::GatherX32),
    vm::MakeNativeFunction("scatter.x8", &VMLAModuleState::ScatterX8),
    vm::MakeNativeFunction("scatter.x16", &VMLAModuleState::ScatterX16),
    vm::MakeNativeFunction("scatter.x32", &VMLAModuleState::ScatterX32),
    vm::MakeNativeFunction("iota.i8", &VMLAModuleState::IotaI8),
    vm::MakeNativeFunction("iota.i16", &VMLAModuleState::IotaI16),
    vm::MakeNativeFunction("iota.i32", &VMLAModuleState::IotaI32),
    vm::MakeNativeFunction("iota.f32", &VMLAModuleState::IotaF32),
    vm::MakeNativeFunction("tile.x8", &VMLAModuleState::TileX8),
    vm::MakeNativeFunction("tile.x16", &VMLAModuleState::TileX16),
    vm::MakeNativeFunction("tile.x32", &VMLAModuleState::TileX32),

    vm::MakeNativeFunction("not.x8", &VMLAModuleState::NotX8),
    vm::MakeNativeFunction("not.x16", &VMLAModuleState::NotX16),
    vm::MakeNativeFunction("not.x32", &VMLAModuleState::NotX32),
    vm::MakeNativeFunction("and.x8", &VMLAModuleState::AndX8),
    vm::MakeNativeFunction("and.x16", &VMLAModuleState::AndX16),
    vm::MakeNativeFunction("and.x32", &VMLAModuleState::AndX32),
    vm::MakeNativeFunction("and.broadcast.x8",
                           &VMLAModuleState::AndBroadcastX8),
    vm::MakeNativeFunction("and.broadcast.x16",
                           &VMLAModuleState::AndBroadcastX16),
    vm::MakeNativeFunction("and.broadcast.x32",
                           &VMLAModuleState::AndBroadcastX32),
    vm::MakeNativeFunction("or.x8", &VMLAModuleState::OrX8),
    vm::MakeNativeFunction("or.x16", &VMLAModuleState::OrX16),
    vm::MakeNativeFunction("or.x32", &VMLAModuleState::OrX32),
    vm::MakeNativeFunction("xor.x8", &VMLAModuleState::XorX8),
    vm::MakeNativeFunction("xor.x16", &VMLAModuleState::XorX16),
    vm::MakeNativeFunction("xor.x32", &VMLAModuleState::XorX32),
    vm::MakeNativeFunction("xor.broadcast.x8",
                           &VMLAModuleState::XorBroadcastX8),
    vm::MakeNativeFunction("xor.broadcast.x16",
                           &VMLAModuleState::XorBroadcastX16),
    vm::MakeNativeFunction("xor.broadcast.x32",
                           &VMLAModuleState::XorBroadcastX32),
    vm::MakeNativeFunction("shl.x8", &VMLAModuleState::ShlX8),
    vm::MakeNativeFunction("shl.x16", &VMLAModuleState::ShlX16),
    vm::MakeNativeFunction("shl.x32", &VMLAModuleState::ShlX32),
    vm::MakeNativeFunction("shr.u8", &VMLAModuleState::ShrU8),
    vm::MakeNativeFunction("shr.u16", &VMLAModuleState::ShrU16),
    vm::MakeNativeFunction("shr.u32", &VMLAModuleState::ShrU32),
    vm::MakeNativeFunction("shr.i8", &VMLAModuleState::ShrI8),
    vm::MakeNativeFunction("shr.i16", &VMLAModuleState::ShrI16),
    vm::MakeNativeFunction("shr.i32", &VMLAModuleState::ShrI32),

    vm::MakeNativeFunction("add.i8", &VMLAModuleState::AddI8),
    vm::MakeNativeFunction("add.i16", &VMLAModuleState::AddI16),
    vm::MakeNativeFunction("add.i32", &VMLAModuleState::AddI32),
    vm::MakeNativeFunction("add.f32", &VMLAModuleState::AddF32),
    vm::MakeNativeFunction("sub.i8", &VMLAModuleState::SubI8),
    vm::MakeNativeFunction("sub.i16", &VMLAModuleState::SubI16),
    vm::MakeNativeFunction("sub.i32", &VMLAModuleState::SubI32),
    vm::MakeNativeFunction("sub.f32", &VMLAModuleState::SubF32),
    vm::MakeNativeFunction("abs.i8", &VMLAModuleState::AbsI8),
    vm::MakeNativeFunction("abs.i16", &VMLAModuleState::AbsI16),
    vm::MakeNativeFunction("abs.i32", &VMLAModuleState::AbsI32),
    vm::MakeNativeFunction("abs.f32", &VMLAModuleState::AbsF32),
    vm::MakeNativeFunction("neg.i8", &VMLAModuleState::NegI8),
    vm::MakeNativeFunction("neg.i16", &VMLAModuleState::NegI16),
    vm::MakeNativeFunction("neg.i32", &VMLAModuleState::NegI32),
    vm::MakeNativeFunction("neg.f32", &VMLAModuleState::NegF32),
    vm::MakeNativeFunction("mul.i8", &VMLAModuleState::MulI8),
    vm::MakeNativeFunction("mul.i16", &VMLAModuleState::MulI16),
    vm::MakeNativeFunction("mul.i32", &VMLAModuleState::MulI32),
    vm::MakeNativeFunction("mul.f32", &VMLAModuleState::MulF32),
    vm::MakeNativeFunction("div.i8", &VMLAModuleState::DivI8),
    vm::MakeNativeFunction("div.i16", &VMLAModuleState::DivI16),
    vm::MakeNativeFunction("div.i32", &VMLAModuleState::DivI32),
    vm::MakeNativeFunction("div.u8", &VMLAModuleState::DivU8),
    vm::MakeNativeFunction("div.u16", &VMLAModuleState::DivU16),
    vm::MakeNativeFunction("div.u32", &VMLAModuleState::DivU32),
    vm::MakeNativeFunction("div.f32", &VMLAModuleState::DivF32),
    vm::MakeNativeFunction("rem.i8", &VMLAModuleState::RemI8),
    vm::MakeNativeFunction("rem.i16", &VMLAModuleState::RemI16),
    vm::MakeNativeFunction("rem.i32", &VMLAModuleState::RemI32),
    vm::MakeNativeFunction("rem.u8", &VMLAModuleState::RemU8),
    vm::MakeNativeFunction("rem.u16", &VMLAModuleState::RemU16),
    vm::MakeNativeFunction("rem.u32", &VMLAModuleState::RemU32),
    vm::MakeNativeFunction("rem.f32", &VMLAModuleState::RemF32),
    vm::MakeNativeFunction("pow.f32", &VMLAModuleState::PowF32),
    vm::MakeNativeFunction("exp.f32", &VMLAModuleState::ExpF32),
    vm::MakeNativeFunction("log.f32", &VMLAModuleState::LogF32),
    vm::MakeNativeFunction("rsqrt.f32", &VMLAModuleState::RsqrtF32),
    vm::MakeNativeFunction("sqrt.f32", &VMLAModuleState::SqrtF32),
    vm::MakeNativeFunction("cos.f32", &VMLAModuleState::CosF32),
    vm::MakeNativeFunction("sin.f32", &VMLAModuleState::SinF32),
    vm::MakeNativeFunction("tanh.f32", &VMLAModuleState::TanhF32),
    vm::MakeNativeFunction("atan2.f32", &VMLAModuleState::Atan2F32),

    vm::MakeNativeFunction("min.i8", &VMLAModuleState::MinI8),
    vm::MakeNativeFunction("min.i16", &VMLAModuleState::MinI16),
    vm::MakeNativeFunction("min.i32", &VMLAModuleState::MinI32),
    vm::MakeNativeFunction("min.f32", &VMLAModuleState::MinF32),
    vm::MakeNativeFunction("max.i8", &VMLAModuleState::MaxI8),
    vm::MakeNativeFunction("max.i16", &VMLAModuleState::MaxI16),
    vm::MakeNativeFunction("max.i32", &VMLAModuleState::MaxI32),
    vm::MakeNativeFunction("max.f32", &VMLAModuleState::MaxF32),
    vm::MakeNativeFunction("clamp.i8", &VMLAModuleState::ClampI8),
    vm::MakeNativeFunction("clamp.i16", &VMLAModuleState::ClampI16),
    vm::MakeNativeFunction("clamp.i32", &VMLAModuleState::ClampI32),
    vm::MakeNativeFunction("clamp.f32", &VMLAModuleState::ClampF32),
    vm::MakeNativeFunction("floor.f32", &VMLAModuleState::FloorF32),
    vm::MakeNativeFunction("ceil.f32", &VMLAModuleState::CeilF32),
    vm::MakeNativeFunction("round.f32", &VMLAModuleState::RoundF32),
    vm::MakeNativeFunction("sort.i8", &VMLAModuleState::SortI8),
    vm::MakeNativeFunction("sort.i16", &VMLAModuleState::SortI16),
    vm::MakeNativeFunction("sort.i32", &VMLAModuleState::SortI32),
    vm::MakeNativeFunction("sort.f32", &VMLAModuleState::SortF32),
    vm::MakeNativeFunction("fft.f32", &VMLAModuleState::FftF32),
    vm::MakeNativeFunction("ifft.f32", &VMLAModuleState::IfftF32),
    vm::MakeNativeFunction("rfft.f32", &VMLAModuleState::RfftF32),
    vm::MakeNativeFunction("irfft.f32", &VMLAModuleState::IrfftF32),
    vm::MakeNativeFunction("finite.f32", &VMLAModuleState::FiniteF32),

    vm::MakeNativeFunction("convert.i8.i16", &VMLAModuleState::ConvertI8I16),
    vm::MakeNativeFunction("convert.i8.i32", &VMLAModuleState::ConvertI8I32),
    vm::MakeNativeFunction("convert.i8.f32", &VMLAModuleState::ConvertI8F32),
    vm::MakeNativeFunction("convert.i16.i8", &VMLAModuleState::ConvertI16I8),
    vm::MakeNativeFunction("convert.i16.i32", &VMLAModuleState::ConvertI16I32),
    vm::MakeNativeFunction("convert.i16.f32", &VMLAModuleState::ConvertI16F32),
    vm::MakeNativeFunction("convert.i32.i8", &VMLAModuleState::ConvertI32I8),
    vm::MakeNativeFunction("convert.i32.i16", &VMLAModuleState::ConvertI32I16),
    vm::MakeNativeFunction("convert.i32.f32", &VMLAModuleState::ConvertI32F32),
    vm::MakeNativeFunction("convert.f32.i8", &VMLAModuleState::ConvertF32I8),
    vm::MakeNativeFunction("convert.f32.i16", &VMLAModuleState::ConvertF32I16),
    vm::MakeNativeFunction("convert.f32.i32", &VMLAModuleState::ConvertF32I32),

    vm::MakeNativeFunction("reduce.sum.i8", &VMLAModuleState::ReduceSumI8),
    vm::MakeNativeFunction("reduce.sum.i16", &VMLAModuleState::ReduceSumI16),
    vm::MakeNativeFunction("reduce.sum.i32", &VMLAModuleState::ReduceSumI32),
    vm::MakeNativeFunction("reduce.sum.f32", &VMLAModuleState::ReduceSumF32),
    vm::MakeNativeFunction("reduce.min.i8", &VMLAModuleState::ReduceMinI8),
    vm::MakeNativeFunction("reduce.min.i16", &VMLAModuleState::ReduceMinI16),
    vm::MakeNativeFunction("reduce.min.i32", &VMLAModuleState::ReduceMinI32),
    vm::MakeNativeFunction("reduce.min.f32", &VMLAModuleState::ReduceMinF32),
    vm::MakeNativeFunction("reduce.max.i8", &VMLAModuleState::ReduceMaxI8),
    vm::MakeNativeFunction("reduce.max.i16", &VMLAModuleState::ReduceMaxI16),
    vm::MakeNativeFunction("reduce.max.i32", &VMLAModuleState::ReduceMaxI32),
    vm::MakeNativeFunction("reduce.max.f32", &VMLAModuleState::ReduceMaxF32),
    vm::MakeNativeFunction("reduce.and.i8", &VMLAModuleState::ReduceAndI8),
    vm::MakeNativeFunction("reduce.or.i8", &VMLAModuleState::ReduceOrI8),

    vm::MakeNativeFunction("pooling.sum.i8", &VMLAModuleState::PoolingSumI8),
    vm::MakeNativeFunction("pooling.sum.i16", &VMLAModuleState::PoolingSumI16),
    vm::MakeNativeFunction("pooling.sum.i32", &VMLAModuleState::PoolingSumI32),
    vm::MakeNativeFunction("pooling.sum.f32", &VMLAModuleState::PoolingSumF32),
    vm::MakeNativeFunction("pooling.min.i8", &VMLAModuleState::PoolingMinI8),
    vm::MakeNativeFunction("pooling.min.i16", &VMLAModuleState::PoolingMinI16),
    vm::MakeNativeFunction("pooling.min.i32", &VMLAModuleState::PoolingMinI32),
    vm::MakeNativeFunction("pooling.min.f32", &VMLAModuleState::PoolingMinF32),
    vm::MakeNativeFunction("pooling.max.i8", &VMLAModuleState::PoolingMaxI8),
    vm::MakeNativeFunction("pooling.max.i16", &VMLAModuleState::PoolingMaxI16),
    vm::MakeNativeFunction("pooling.max.i32", &VMLAModuleState::PoolingMaxI32),
    vm::MakeNativeFunction("pooling.max.f32", &VMLAModuleState::PoolingMaxF32),

    vm::MakeNativeFunction(
        "batch.matmul.f32f32.f32",
        &VMLAModuleState::BatchMatMul<float, float, float, float>),

    vm::MakeNativeFunction(
        "batch.matmul.i32i32.i32",
        &VMLAModuleState::BatchMatMul<int32_t, int32_t, int32_t, int32_t>),

    vm::MakeNativeFunction(
        "batch.matmul.i8i8.i32",
        &VMLAModuleState::BatchMatMul<int8_t, int8_t, int32_t, int32_t>),

    vm::MakeNativeFunction(
        "batch.matmul.i16i16.i32",
        &VMLAModuleState::BatchMatMul<int16_t, int16_t, int32_t, int32_t>),

    vm::MakeNativeFunction("conv.f32f32.f32", &VMLAModuleState::ConvF32F32F32)};

// Per-device VMLA module.
// One of these will be created per device and be shared across all executables
// that are created within that device. Large shared kernel state can go here
// (such as thread pools/caches/etc), though note that they must be either
// thread-safe or internally synchronized.
//
// Thread-safe.
class VMLAModule final : public vm::NativeModule<VMLAModuleState> {
 public:
  explicit VMLAModule(iree_allocator_t allocator)
      : vm::NativeModule<VMLAModuleState>(
            "vmla", allocator, absl::MakeConstSpan(kVMLAModuleFunctions)) {}
  ~VMLAModule() = default;

  Status Initialize() {
    IREE_TRACE_SCOPE0("VMLAModule::Initialize");
    return OkStatus();
  }

  StatusOr<std::unique_ptr<VMLAModuleState>> CreateState(
      iree_allocator_t allocator) override {
    IREE_TRACE_SCOPE0("VMLAModule::CreateState");
    auto state = std::make_unique<VMLAModuleState>(allocator, &kernel_state_);
    return state;
  }

 private:
  // NOTE: shared across all contexts with the VMLA module loaded. See
  // VMLAModuleState::kernel_state_ for more information.
  kernels::RuntimeState kernel_state_;
};

}  // namespace

Status ModuleCreate(iree_allocator_t allocator, iree_vm_module_t** out_module) {
  if (!out_module) {
    return iree_make_status(IREE_STATUS_INVALID_ARGUMENT,
                            "out_module must not be null");
  }
  *out_module = nullptr;
  auto module = std::make_unique<VMLAModule>(allocator);
  IREE_RETURN_IF_ERROR(module->Initialize());
  *out_module = module.release()->interface();
  return OkStatus();
}

}  // namespace vmla
}  // namespace hal
}  // namespace iree

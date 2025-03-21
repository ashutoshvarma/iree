// Copyright 2020 Google LLC
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

#include "iree/hal/vulkan/timepoint_util.h"

#include <memory>

#include "absl/synchronization/mutex.h"
#include "iree/base/tracing.h"
#include "iree/hal/vulkan/dynamic_symbols.h"
#include "iree/hal/vulkan/status_util.h"

namespace iree {
namespace hal {
namespace vulkan {

// static
void TimePointFence::Delete(TimePointFence* ptr) {
  ptr->ResetStatus();
  ptr->pool()->ReleaseResolved(ptr);
}

VkResult TimePointFence::GetStatus() {
  absl::MutexLock lock(&status_mutex_);
  if (status_ == VK_NOT_READY) {
    const auto& device = pool()->logical_device();
    status_ = device->syms()->vkGetFenceStatus(*device, fence_);
  }
  return status_;
}

void TimePointFence::ResetStatus() {
  absl::MutexLock lock(&status_mutex_);
  status_ = VK_NOT_READY;
}

// static
iree_status_t TimePointFencePool::Create(VkDeviceHandle* logical_device,
                                         TimePointFencePool** out_pool) {
  IREE_TRACE_SCOPE0("TimePointFencePool::Create");
  ref_ptr<TimePointFencePool> pool(new TimePointFencePool(logical_device));
  IREE_RETURN_IF_ERROR(pool->PreallocateFences());
  *out_pool = pool.release();
  return iree_ok_status();
}

TimePointFencePool::~TimePointFencePool() {
  IREE_TRACE_SCOPE0("TimePointFencePool::dtor");

  absl::MutexLock lock(&mutex_);
  int free_count = 0;
  for (auto* fence : free_fences_) {
    syms()->vkDestroyFence(*logical_device_, fence->value(),
                           logical_device_->allocator());
    ++free_count;
  }
  IREE_DCHECK_EQ(free_count, kMaxInFlightFenceCount);
  free_fences_.clear();
}

Status TimePointFencePool::Acquire(ref_ptr<TimePointFence>* out_fence) {
  IREE_TRACE_SCOPE0("TimePointFencePool::Acquire");

  absl::MutexLock lock(&mutex_);
  if (free_fences_.empty()) {
    return iree_make_status(IREE_STATUS_RESOURCE_EXHAUSTED,
                            "fence pool out of free fences");
  }

  // To acquire from the pool, we:
  //   1) Pop from the front of the queue (reference count of 0);
  //   2) Release the unique_ptr, since lifetime will be managed by ref counts;
  //   3) Return as a raw RefObject with a reference count of 1;
  // When the reference count goes back to 0, it will be returned to the pool,
  // wrapped with unique_ptr.
  // When the pool is destroyed, all free fences are freed by unique_ptr
  // automatically.
  std::unique_ptr<TimePointFence> fence =
      free_fences_.take(free_fences_.front());
  *out_fence = add_ref(fence.release());
  return OkStatus();
}

void TimePointFencePool::ReleaseResolved(TimePointFence* fence) {
  IREE_TRACE_SCOPE0("TimePointFencePool::ReleaseResolved");
  VkFence f = fence->value();
  syms()->vkResetFences(*logical_device_, 1, &f);
  absl::MutexLock lock(&mutex_);
  free_fences_.push_back(std::unique_ptr<TimePointFence>(fence));
}

TimePointFencePool::TimePointFencePool(VkDeviceHandle* logical_device)
    : logical_device_(logical_device) {}

const ref_ptr<DynamicSymbols>& TimePointFencePool::syms() const {
  return logical_device_->syms();
}

Status TimePointFencePool::PreallocateFences() {
  IREE_TRACE_SCOPE0("TimePointFencePool::PreallocateFences");

  VkFenceCreateInfo create_info;
  create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  create_info.pNext = nullptr;
  create_info.flags = 0;

  std::array<std::unique_ptr<TimePointFence>, kMaxInFlightFenceCount> fences;
  {
    absl::MutexLock lock(&mutex_);
    for (int i = 0; i < fences.size(); ++i) {
      VkFence fence = VK_NULL_HANDLE;
      VK_RETURN_IF_ERROR(
          syms()->vkCreateFence(*logical_device_, &create_info,
                                logical_device_->allocator(), &fence),
          "vkCreateFence");
      fences[i] = std::make_unique<TimePointFence>(this, fence);
    }
  }

  for (int i = 0; i < fences.size(); ++i) {
    // The `TimePointFence`s was created with an initial ref-count of one.
    // Decrease explicitly to zero so that later we can rely on the ref-count
    // reaching zero to auto-release the `TimePointFence` back to the free
    // list. As a nice side effect, this will also initialize the free list
    // with all newly created fences.
    // TODO: Might want to avoid acquiring and releasing the mutex for each
    // fence.
    fences[i].release()->ReleaseReference();
  }

  return OkStatus();
}

// static
iree_status_t TimePointSemaphorePool::Create(
    VkDeviceHandle* logical_device, TimePointSemaphorePool** out_pool) {
  IREE_TRACE_SCOPE0("TimePointSemaphorePool::Create");
  ref_ptr<TimePointSemaphorePool> pool(
      new TimePointSemaphorePool(logical_device));
  IREE_RETURN_IF_ERROR(pool->PreallocateSemaphores());
  *out_pool = pool.release();
  return iree_ok_status();
}

TimePointSemaphorePool::~TimePointSemaphorePool() {
  IREE_TRACE_SCOPE0("TimePointSemaphorePool::dtor");

  absl::MutexLock lock(&mutex_);

  IREE_DCHECK_EQ(free_semaphores_.size(), kMaxInFlightSemaphoreCount);
  free_semaphores_.clear();

  for (auto& semaphore : storage_) {
    syms()->vkDestroySemaphore(*logical_device_, semaphore.semaphore,
                               logical_device_->allocator());
  }
}

Status TimePointSemaphorePool::Acquire(TimePointSemaphore** out_semaphore) {
  IREE_TRACE_SCOPE0("TimePointSemaphorePool::Acquire");

  absl::MutexLock lock(&mutex_);
  if (free_semaphores_.empty()) {
    return iree_make_status(IREE_STATUS_RESOURCE_EXHAUSTED,
                            "semaphore pool out of free semaphores");
  }

  *out_semaphore = free_semaphores_.front();
  free_semaphores_.pop_front();
  return OkStatus();
}

void TimePointSemaphorePool::ReleaseResolved(
    IntrusiveList<TimePointSemaphore>* semaphores) {
  IREE_TRACE_SCOPE0("TimePointSemaphorePool::ReleaseResolved");

  for (auto* semaphore : *semaphores) {
    IREE_DCHECK(!semaphore->signal_fence && !semaphore->wait_fence);
    semaphore->value = UINT64_MAX;
  }

  absl::MutexLock lock(&mutex_);
  free_semaphores_.merge_from(semaphores);
}

void TimePointSemaphorePool::ReleaseUnresolved(
    IntrusiveList<TimePointSemaphore>* semaphores) {
  IREE_TRACE_SCOPE0("TimePointSemaphorePool::ReleaseUnresolved");

  for (auto* semaphore : *semaphores) {
    semaphore->signal_fence = nullptr;
    semaphore->wait_fence = nullptr;
    semaphore->value = UINT64_MAX;
  }

  absl::MutexLock lock(&mutex_);
  free_semaphores_.merge_from(semaphores);
}

TimePointSemaphorePool::TimePointSemaphorePool(VkDeviceHandle* logical_device)
    : logical_device_(logical_device) {}

const ref_ptr<DynamicSymbols>& TimePointSemaphorePool::syms() const {
  return logical_device_->syms();
}

Status TimePointSemaphorePool::PreallocateSemaphores() {
  IREE_TRACE_SCOPE0("TimePointSemaphorePool::PreallocateSemaphores");

  VkSemaphoreCreateInfo create_info;
  create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  create_info.pNext = nullptr;
  create_info.flags = 0;

  absl::MutexLock lock(&mutex_);
  for (int i = 0; i < kMaxInFlightSemaphoreCount; ++i) {
    auto* semaphore = &storage_[i];
    VK_RETURN_IF_ERROR(syms()->vkCreateSemaphore(*logical_device_, &create_info,
                                                 logical_device_->allocator(),
                                                 &semaphore->semaphore),
                       "vkCreateSemaphore");
    free_semaphores_.push_back(semaphore);
  }

  return OkStatus();
}

}  // namespace vulkan
}  // namespace hal
}  // namespace iree

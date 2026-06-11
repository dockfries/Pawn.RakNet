/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2023 katursis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "main.h"

BitStream *BitStreamPool::Alloc() {
  for (auto &[bs, is_occupied] : items_) {
    if (!is_occupied) {
      is_occupied = true;

      return bs.get();
    }
  }

  const auto &[bs, is_occupied] =
      items_.emplace_back(std::make_shared<BitStream>(), true);

  return bs.get();
}

void BitStreamPool::Free(BitStream *ptr) {
  for (auto &[bs, is_occupied] : items_) {
    if (bs.get() == ptr) {
      bs->reset();

      is_occupied = false;

      return;
    }
  }
}

cell BitStreamPool::New() {
  std::lock_guard<std::mutex> lock(mutex_);

  auto ptr = Alloc();

  cell handle;
  if (!free_handles_.empty()) {
    handle = free_handles_.front();
    free_handles_.pop();
  } else {
    handle = ++next_handle_;
  }

  handles_[handle] = ptr;

  return handle;
}

BitStream *BitStreamPool::Get(cell handle) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = handles_.find(handle);
  return it != handles_.end() ? it->second : nullptr;
}

void BitStreamPool::Delete(cell handle) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = handles_.find(handle);
  if (it == handles_.end()) {
    return;
  }

  Free(it->second);
  handles_.erase(it);
  free_handles_.push(handle);
}

cell BitStreamPool::GetHandle(BitStream *ptr) {
  std::lock_guard<std::mutex> lock(mutex_);

  for (auto &[handle, bs_ptr] : handles_) {
    if (bs_ptr == ptr) {
      return handle;
    }
  }

  return 0;
}

cell BitStreamPool::AddExternal(BitStream *ptr) {
  std::lock_guard<std::mutex> lock(mutex_);

  cell handle;
  if (!free_handles_.empty()) {
    handle = free_handles_.front();
    free_handles_.pop();
  } else {
    handle = ++next_handle_;
  }

  handles_[handle] = ptr;

  return handle;
}

void BitStreamPool::RemoveExternal(cell handle) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = handles_.find(handle);
  if (it == handles_.end()) {
    return;
  }

  handles_.erase(it);
  free_handles_.push(handle);
}

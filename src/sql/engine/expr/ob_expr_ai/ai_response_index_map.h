/**
 * Copyright (c) 2025 OceanBase.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OCEANBASE_SQL_ENGINE_EXPR_OB_EXPR_AI_AI_RESPONSE_INDEX_MAP_H_
#define OCEANBASE_SQL_ENGINE_EXPR_OB_EXPR_AI_AI_RESPONSE_INDEX_MAP_H_

#include <stdint.h>
#include <vector>

namespace oceanbase
{
namespace common
{

// Tracks which response slot carries the answer of which request for a
// batched AI invocation whose responses may arrive out of order.
//
// The caller owns the backing array: response_positions_[response_index]
// stores the request index whose result lives at response_index. Both
// sides of the mapping are guarded, so record() rejects out-of-range
// request or response indices, a response slot that is already taken and
// a request that has already been recorded. complete() turns true once
// every slot has been filled exactly once.
class AiResponseIndexMap
{
public:
  AiResponseIndexMap(const int64_t request_count, int64_t *response_positions)
      : request_count_(request_count > 0 ? request_count : 0),
        response_positions_(response_positions),
        slot_filled_(),
        request_filled_(),
        filled_count_(0)
  {
    if (request_count_ > 0) {
      slot_filled_.assign(static_cast<size_t>(request_count_), 0);
      request_filled_.assign(static_cast<size_t>(request_count_), 0);
    }
  }

  // Binds response_positions_[response_index] to request_index. Returns
  // false without touching any state when the storage is unavailable, an
  // index is out of range, or the slot/request is already bound.
  bool record(const int64_t request_index, const int64_t response_index)
  {
    if (nullptr == response_positions_ || !is_valid_index(request_index)
        || !is_valid_index(response_index)) {
      return false;
    }
    const size_t slot = static_cast<size_t>(response_index);
    const size_t request = static_cast<size_t>(request_index);
    if (0 != slot_filled_[slot] || 0 != request_filled_[request]) {
      return false;
    }
    response_positions_[slot] = request_index;
    slot_filled_[slot] = 1;
    request_filled_[request] = 1;
    ++filled_count_;
    return true;
  }

  // Returns the request index bound to response_index, or -1 when the
  // index is out of range, the storage is unavailable or the slot has not
  // been filled yet.
  int64_t response_position(const int64_t response_index) const
  {
    if (nullptr == response_positions_ || !is_valid_index(response_index)
        || 0 == slot_filled_[static_cast<size_t>(response_index)]) {
      return -1;
    }
    return response_positions_[response_index];
  }

  // True only when the backing storage is available and every response
  // slot has been bound exactly once.
  bool complete() const
  {
    return nullptr != response_positions_ && request_count_ > 0 && filled_count_ == request_count_;
  }

  AiResponseIndexMap(const AiResponseIndexMap &) = delete;
  AiResponseIndexMap &operator=(const AiResponseIndexMap &) = delete;

private:
  bool is_valid_index(const int64_t index) const
  {
    return 0 <= index && index < request_count_;
  }

  int64_t request_count_;
  int64_t *response_positions_;
  std::vector<int8_t> slot_filled_;
  std::vector<int8_t> request_filled_;
  int64_t filled_count_;
};

} // end namespace common
} // end namespace oceanbase

#endif // OCEANBASE_SQL_ENGINE_EXPR_OB_EXPR_AI_AI_RESPONSE_INDEX_MAP_H_

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

#ifndef OCEANBASE_SQL_ENGINE_EXPR_OB_EXPR_AI_AI_PROMPT_TEMPLATE_H_
#define OCEANBASE_SQL_ENGINE_EXPR_OB_EXPR_AI_AI_PROMPT_TEMPLATE_H_

#include <stdint.h>

namespace oceanbase
{
namespace common
{

// Walks an AI prompt template and streams its segments to the caller.
//
// A template mixes literal text with {N} placeholders, where N is a
// non-negative decimal index into the argument list. Braces are escaped by
// doubling them: "{{" and "}}" denote the literal characters "{" and "}"
// respectively.
//
// For every literal run the visitor is invoked with index == -1 and the
// (pointer, length) span of the literal bytes. For every placeholder the
// visitor is invoked with the parsed index and the raw "{N}" span. The
// visitor may return false at any time to abort the walk, which makes
// visit_ai_prompt_template() return false.
//
// Returns false (without invoking the visitor) when the template is
// malformed: an unclosed placeholder, a placeholder that is empty or holds
// a non-digit character, an index that overflows int64_t or is not smaller
// than arg_count, or a "}" that is neither doubled nor part of a
// placeholder.
template <typename Visitor>
inline bool visit_ai_prompt_template(const char *data, const int64_t size,
                                     const int64_t arg_count, Visitor &&visitor)
{
  if (nullptr == data && 0 != size) {
    return false;
  }
  if (0 > arg_count) {
    return false;
  }  bool valid = true;
  int64_t pos = 0;
  while (valid && pos < size) {
    if ('{' == data[pos]) {
      if (pos + 1 < size && '{' == data[pos + 1]) {
        valid = visitor(data + pos, 1, -1);
        pos += 2;
      } else {
        // placeholder: at least one digit, closed by '}'
        int64_t end = pos + 1;
        int64_t value = 0;
        bool overflow = false;
        while (end < size && '}' != data[end]) {
          if (data[end] < '0' || data[end] > '9') {
            return false;
          }
          const int64_t digit = static_cast<int64_t>(data[end] - '0');
          if (value > (INT64_MAX - digit) / 10) {
            overflow = true;
          } else {
            value = value * 10 + digit;
          }
          ++end;
        }
        if (end >= size || end == pos + 1 || overflow || value >= arg_count) {
          return false;
        }
        valid = visitor(data + pos, end + 1 - pos, value);
        pos = end + 1;
      }
    } else if ('}' == data[pos]) {
      if (pos + 1 < size && '}' == data[pos + 1]) {
        valid = visitor(data + pos, 1, -1);
        pos += 2;
      } else {
        return false;
      }
    } else {
      int64_t end = pos;
      while (end < size && '{' != data[end] && '}' != data[end]) {
        ++end;
      }
      valid = visitor(data + pos, end - pos, -1);
      pos = end;
    }
  }
  return valid;
}

} // end namespace common
} // end namespace oceanbase

#endif // OCEANBASE_SQL_ENGINE_EXPR_OB_EXPR_AI_AI_PROMPT_TEMPLATE_H_

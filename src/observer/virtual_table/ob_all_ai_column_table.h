/*
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

#ifndef OCEANBASE_OBSERVER_VIRTUAL_TABLE_OB_ALL_AI_COLUMN_TABLE_
#define OCEANBASE_OBSERVER_VIRTUAL_TABLE_OB_ALL_AI_COLUMN_TABLE_

#include "observer/virtual_table/ob_virtual_table_scanner_iterator.h"

namespace oceanbase
{
namespace share
{
namespace schema
{
class ObMultiVersionSchemaService;
}
}
namespace observer
{
// __all_ai_column: one row per AI column of every user table, materialized
// from the column schema AI meta. The three hidden bookkeeping columns of an
// AI column are looked up by name (`__ai_status_<col>`, `__ai_request_<col>`,
// `__ai_result_spec_<col>`), so this works for both CREATE TABLE and
// ALTER TABLE ADD COLUMN paths.
class ObAllAIColumnTable : public common::ObVirtualTableScannerIterator
{
public:
  explicit ObAllAIColumnTable(share::schema::ObMultiVersionSchemaService &schema_service)
      : schema_service_(schema_service), rows_(), row_idx_(0), loaded_(false) {}
  virtual ~ObAllAIColumnTable() {}
  virtual int inner_get_next_row(common::ObNewRow *&row) override;
  virtual void reset() override;

private:
  struct AIColumnRow
  {
    uint64_t table_id_;
    uint64_t column_id_;
    common::ObString column_name_;
    common::ObString model_name_;
    common::ObString sync_mode_;
    int64_t max_retries_;
    common::ObString normalized_expr_;
    uint64_t state_column_id_;
    uint64_t request_column_id_;
    uint64_t result_spec_column_id_;
    AIColumnRow() { reset(); }
    void reset()
    {
      table_id_ = OB_INVALID_ID;
      column_id_ = OB_INVALID_ID;
      column_name_.reset();
      model_name_.reset();
      sync_mode_.reset();
      max_retries_ = 0;
      normalized_expr_.reset();
      state_column_id_ = OB_INVALID_ID;
      request_column_id_ = OB_INVALID_ID;
      result_spec_column_id_ = OB_INVALID_ID;
    }
  };

  int load_rows_();

private:
  static const int64_t MAX_ROW_COUNT = 4096;
  share::schema::ObMultiVersionSchemaService &schema_service_;
  common::ObArenaAllocator allocator_;
  common::ObSEArray<AIColumnRow, 64> rows_;
  int64_t row_idx_;
  bool loaded_;

  enum COLUMN_ID
  {
    TABLE_ID = common::OB_APP_MIN_COLUMN_ID,
    COLUMN_ID,
    COLUMN_NAME,
    AI_MODEL_NAME,
    SYNC_MODE,
    MAX_RETRIES,
    NORMALIZED_EXPR,
    STATE_COLUMN_ID,
    REQUEST_COLUMN_ID,
    RESULT_SPEC_COLUMN_ID,
  };
}; // class ObAllAIColumnTable
} // namespace observer
} // namespace oceanbase
#endif // OCEANBASE_OBSERVER_VIRTUAL_TABLE_OB_ALL_AI_COLUMN_TABLE_

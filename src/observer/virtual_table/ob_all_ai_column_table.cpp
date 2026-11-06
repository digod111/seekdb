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

#define USING_LOG_PREFIX SERVER

#include "observer/virtual_table/ob_all_ai_column_table.h"
#include "lib/allocator/page_arena.h"
#include "lib/string/ob_sql_string.h"
#include "share/schema/ob_multi_version_schema_service.h"
#include "share/schema/ob_schema_getter_guard.h"
#include "share/schema/ob_table_schema.h"
#include "share/schema/ob_column_schema.h"

using namespace oceanbase::common;
using namespace oceanbase::share;
using namespace oceanbase::share::schema;

namespace oceanbase
{
namespace observer
{

ObAllAIColumnTable::ObAllAIColumnTable(ObMultiVersionSchemaService &schema_service)
    : schema_service_(schema_service),
      allocator_("AIColVT"),
      rows_(),
      row_idx_(0),
      loaded_(false)
{
}

void ObAllAIColumnTable::reset()
{
  ObVirtualTableScannerIterator::reset();
  rows_.reset();
  row_idx_ = 0;
  loaded_ = false;
  allocator_.reset();
}

int ObAllAIColumnTable::inner_get_next_row(ObNewRow *&row)
{
  int ret = OB_SUCCESS;
  if (OB_UNLIKELY(!loaded_)) {
    if (OB_FAIL(load_rows_())) {
      LOG_WARN("fail to load ai column rows", K(ret));
    } else {
      loaded_ = true;
    }
  }
  if (OB_SUCC(ret)) {
    if (row_idx_ >= rows_.count()) {
      ret = OB_ITER_END;
    } else {
      const AIColumnRow &ai_row = rows_.at(row_idx_++);
      ObObj *cells = cur_row_.cells_;
      if (OB_ISNULL(cells)) {
        ret = OB_ERR_UNEXPECTED;
        LOG_WARN("cur row cell is NULL", K(ret));
      } else {
        const int64_t col_count = output_column_ids_.count();
        for (int64_t i = 0; OB_SUCC(ret) && i < col_count; ++i) {
          const uint64_t col_id = output_column_ids_.at(i);
          switch (col_id) {
            case TABLE_ID: {
              cells[i].set_int(ai_row.table_id_);
              break;
            }
            case COLUMN_ID: {
              cells[i].set_int(ai_row.column_id_);
              break;
            }
            case COLUMN_NAME: {
              cells[i].set_varchar(ai_row.column_name_);
              cells[i].set_collation_type(CS_TYPE_UTF8MB4_GENERAL_CI);
              break;
            }
            case AI_MODEL_NAME: {
              cells[i].set_varchar(ai_row.model_name_);
              cells[i].set_collation_type(CS_TYPE_UTF8MB4_GENERAL_CI);
              break;
            }
            case SYNC_MODE: {
              cells[i].set_varchar(ai_row.sync_mode_);
              cells[i].set_collation_type(CS_TYPE_UTF8MB4_GENERAL_CI);
              break;
            }
            case MAX_RETRIES: {
              cells[i].set_int(ai_row.max_retries_);
              break;
            }
            case NORMALIZED_EXPR: {
              cells[i].set_varchar(ai_row.normalized_expr_);
              cells[i].set_collation_type(CS_TYPE_UTF8MB4_GENERAL_CI);
              break;
            }
            case STATE_COLUMN_ID: {
              cells[i].set_int(ai_row.state_column_id_);
              break;
            }
            case REQUEST_COLUMN_ID: {
              cells[i].set_int(ai_row.request_column_id_);
              break;
            }
            case RESULT_SPEC_COLUMN_ID: {
              cells[i].set_int(ai_row.result_spec_column_id_);
              break;
            }
            default: {
              ret = OB_ERR_UNEXPECTED;
              LOG_WARN("unexpected column id", K(ret), K(col_id));
            }
          }
        }
        if (OB_SUCC(ret)) {
          row = &cur_row_;
        }
      }
    }
  }
  return ret;
}

int ObAllAIColumnTable::load_rows_()
{
  int ret = OB_SUCCESS;
  ObSchemaGetterGuard guard;
  ObSEArray<uint64_t, 128> table_ids;
  if (OB_FAIL(schema_service_.get_runtime_schema_guard(guard))) {
    LOG_WARN("fail to get schema guard", K(ret));
  } else if (OB_FAIL(guard.get_table_ids_in_runtime(table_ids))) {
    LOG_WARN("fail to get table ids", K(ret));
  }
  for (int64_t i = 0; OB_SUCC(ret) && i < table_ids.count() && rows_.count() < MAX_ROW_COUNT; ++i) {
    const ObTableSchema *table = nullptr;
    if (OB_FAIL(guard.get_table_schema(table_ids.at(i), table))) {
      LOG_WARN("fail to get table schema", K(ret), "table_id", table_ids.at(i));
      ret = OB_SUCCESS; // ignore this table
    } else if (OB_ISNULL(table) || table->is_sys_table() || table->is_index_table()) {
      // do nothing
    } else {
      for (ObTableSchema::const_column_iterator iter = table->column_begin();
           OB_SUCC(ret) && iter != table->column_end() && rows_.count() < MAX_ROW_COUNT; ++iter) {
        const ObColumnSchemaV2 *column = *iter;
        if (OB_ISNULL(column)) {
          ret = OB_ERR_UNEXPECTED;
        } else if (!column->is_ai_column()) {
          // do nothing
        } else {
          AIColumnRow ai_row;
          const ObString ai_name = column->get_column_name_str();
          ai_row.table_id_ = table->get_table_id();
          ai_row.column_id_ = column->get_column_id();
          ai_row.max_retries_ = column->get_ai_meta().max_retries_;
          if (OB_FAIL(ob_write_string(allocator_, ai_name, ai_row.column_name_))) {
            LOG_WARN("fail to copy ai column name", K(ret));
          } else if (OB_FAIL(ob_write_string(allocator_,
                                             column->get_ai_meta().model_name_,
                                             ai_row.model_name_))) {
            LOG_WARN("fail to copy ai model name", K(ret));
          } else if (OB_FAIL(ob_write_string(allocator_,
                                             column->get_ai_meta().sync_mode_,
                                             ai_row.sync_mode_))) {
            LOG_WARN("fail to copy ai sync mode", K(ret));
          } else {
            // hidden bookkeeping columns are located by name, so both the
            // CREATE TABLE path and the ALTER TABLE ADD COLUMN path work
            ObSqlString status_name;
            ObSqlString request_name;
            ObSqlString result_spec_name;
            if (OB_FAIL(status_name.append("__ai_status_"))) {
            } else if (OB_FAIL(status_name.append(ai_name))) {
            } else if (OB_FAIL(request_name.append("__ai_request_"))) {
            } else if (OB_FAIL(request_name.append(ai_name))) {
            } else if (OB_FAIL(result_spec_name.append("__ai_result_spec_"))) {
            } else if (OB_FAIL(result_spec_name.append(ai_name))) {
            }
            const ObColumnSchemaV2 *status_col = nullptr;
            const ObColumnSchemaV2 *request_col = nullptr;
            const ObColumnSchemaV2 *result_spec_col = nullptr;
            if (OB_SUCC(ret)) {
              status_col = table->get_column_schema(ObString(
                  static_cast<int32_t>(status_name.length()), status_name.ptr()));
              request_col = table->get_column_schema(ObString(
                  static_cast<int32_t>(request_name.length()), request_name.ptr()));
              result_spec_col = table->get_column_schema(ObString(
                  static_cast<int32_t>(result_spec_name.length()), result_spec_name.ptr()));
              ai_row.state_column_id_ = nullptr == status_col ? OB_INVALID_ID
                                                              : status_col->get_column_id();
              ai_row.request_column_id_ = nullptr == request_col ? OB_INVALID_ID
                                                                 : request_col->get_column_id();
              ai_row.result_spec_column_id_ = nullptr == result_spec_col
                  ? OB_INVALID_ID : result_spec_col->get_column_id();
            }
            // normalized expression: a typed system prompt followed by the
            // original ai expression
            ObSqlString normalized;
            const ObObjType dt = column->get_data_type();
            const bool is_int_type = (ObIntType == dt || ObTinyIntType == dt
                || ObSmallIntType == dt || ObMediumIntType == dt || ObBigIntType == dt
                || ObUTinyIntType == dt || ObUSmallIntType == dt
                || ObUMediumIntType == dt || ObUBigIntType == dt);
            if (column->is_json()) {
              if (OB_FAIL(normalized.append(
                      "Return exactly one valid JSON value for the following task. "))) {
              } else if (OB_FAIL(normalized.append(column->get_ai_meta().expr_text_))) {
              }
            } else if (is_int_type) {
              if (OB_FAIL(normalized.append(
                      "Return a base-10 integer for the following task. "))) {
              } else if (OB_FAIL(normalized.append(column->get_ai_meta().expr_text_))) {
              }
            } else if (OB_FAIL(normalized.append(column->get_ai_meta().expr_text_))) {
            }
            if (OB_FAIL(ret)) {
              LOG_WARN("fail to build normalized expr", K(ret));
            } else if (OB_FAIL(ob_write_string(allocator_,
                                               ObString(static_cast<int32_t>(normalized.length()),
                                                        normalized.ptr()),
                                               ai_row.normalized_expr_))) {
              LOG_WARN("fail to copy normalized expr", K(ret));
            } else if (OB_FAIL(rows_.push_back(ai_row))) {
              LOG_WARN("fail to add ai column row", K(ret));
            }
          }
        }
      }
    }
  }
  return ret;
}

} // namespace observer
} // namespace oceanbase

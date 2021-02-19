#include "execution.h"

#include "execution_context.h"
#include "pefa/kernels/filter.h"

#include <arrow/api.h>
#include <memory>
#include <pefa/utils/exceptions.h>
#include <pefa/utils/utils.h>
#include <utility>

namespace pefa::execution {
std::shared_ptr<ExecutionContext> project(const std::shared_ptr<ExecutionContext> &ctx,
                                          std::vector<std::string> column_names) {
  std::vector<std::shared_ptr<arrow::ChunkedArray>> columns(column_names.size());
  std::vector<std::shared_ptr<arrow::Field>> fields(column_names.size());
  for (int i = 0; i < column_names.size(); i++) {
    auto col = ctx->table->GetColumnByName(column_names[i]);
    fields[i] = std::make_shared<arrow::Field>(column_names[i], col->type());
    columns[i] = col;
  }
  return std::make_shared<ExecutionContext>(
      arrow::Table::Make(std::make_shared<arrow::Schema>(fields), columns));
}

class FilterExprExecutor : public ExprVisitor {
private:
  std::shared_ptr<ExecutionContext> m_ctx;
  std::shared_ptr<arrow::Buffer> m_buffer;

public:
  explicit FilterExprExecutor(std::shared_ptr<ExecutionContext> ctx)
      : m_ctx(std::move(ctx)) {}
  void visit(const PredicateExpr &expr) override {
    expr.lhs->visit(*this);
    auto lhs_buf = m_buffer;
    expr.rhs->visit(*this);
    auto rhs_buf = m_buffer;
    if (expr.op == PredicateExpr::Op::AND) {
      for (int i = 0; i < lhs_buf->size(); i++) {
        lhs_buf->mutable_data()[i] &= rhs_buf->mutable_data()[i];
      }
    } else {
      for (int i = 0; i < lhs_buf->size(); i++) {
        lhs_buf->mutable_data()[i] |= rhs_buf->mutable_data()[i];
      }
    }
    m_buffer = lhs_buf;
  }

  void visit(const CompareExpr &expr) override {
    // TODO: support expressions like a > b, 3 * a > 5 * b
    // currently we support only expressions like a > <const>
    auto buffer = arrow::AllocateBitmap(m_ctx->table->num_rows()).ValueOrDie();
    std::memset(buffer->mutable_data(), 255, buffer->size());
    // TODO: add check if column exists
    auto field = m_ctx->table->schema()->GetFieldByName(expr.lhs->name);
    auto column = m_ctx->table->GetColumnByName(expr.lhs->name);
    auto kernel = kernels::FilterKernel::create_cpu(field, std::make_shared<CompareExpr>(expr));
    kernel->compile();
    // TODO: make that implementation parallel
    for (int chunk_num = 0; chunk_num < column->num_chunks(); chunk_num++) {
      size_t offset = 0;
      for (int i = 0; i < chunk_num; i++) {
        offset += column->chunk(i)->length();
      }
      // if some byte from bitmap is located between 2 chunks, we calculate how much bits from that
      // byte belongs to previous chunk
      auto prev_bits = offset % 8;
      // this way we can calculate how much bits from that byte belons to current chunk
      // we use % 8 to handle case, when byte completely lies in current chunk, then prev_bits would
      // be equal to 0 and we don't need an offset
      auto remaining_bits = (8 - prev_bits) % 8;
      kernel->execute(column->chunk(chunk_num), buffer->mutable_data() + offset / 8,
                      remaining_bits);
    }
    size_t offset = 0;
    for (int chunk_num = 0; chunk_num < column->num_chunks(); chunk_num++) {
      if (offset % 8 != 0) {
        kernel->execute_remaining(column->chunk(chunk_num), buffer->mutable_data() + offset / 8, 0,
                                  offset % 8);
      }
      offset += column->chunk(chunk_num)->length();
      if (offset % 8 != 0) {
        kernel->execute_remaining(column->chunk(chunk_num), buffer->mutable_data() + offset / 8,
                                  column->chunk(chunk_num)->length() - (offset % 8), 0);
      }
    }
    m_buffer = buffer;
  }

  [[nodiscard]] std::shared_ptr<arrow::Buffer> result() const {
    return m_buffer;
  }
};

std::shared_ptr<ExecutionContext>
generate_filter_bitmap(const std::shared_ptr<ExecutionContext> &ctx,
                       const std::shared_ptr<BooleanExpr> &expr) {
  // TODO: handle empty input
  if (ctx->table->num_columns() == 0 || ctx->table->column(0)->num_chunks() == 0) {
    return std::make_shared<ExecutionContext>(ctx->table);
  }
  FilterExprExecutor expr_executor(ctx);
  expr->visit(expr_executor);

  // TODO: create ExecutionContext from ctx and correctly join filter_bitmaps
  auto res = std::make_shared<ExecutionContext>(ctx->table);
  res->metadata->filter_bitmap = expr_executor.result();
  return res;
}

template <typename T>
std::shared_ptr<arrow::ChunkedArray> materialize_column(const arrow::ChunkedArray &column,
                                                        const uint8_t *bitmap) {
  // TODO: move chunk_size to executionContext config
  const uint64_t chunk_size = 2 << 13;

  auto type = column.type();
  auto total_length = column.length();

  std::vector<std::shared_ptr<arrow::Array>> new_column;
  int current_chunk = 0;
  int current_chunk_pos = 0;
  int total_elements_pos = 0;

  do {
    int new_chunk_pos = 0;
    auto buffer = arrow::AllocateBuffer(sizeof(T) * chunk_size).ValueOrDie();
    auto data_out = reinterpret_cast<T *>(buffer->mutable_data());
    while (new_chunk_pos < chunk_size && total_elements_pos < total_length) {
      auto &chunk = *column.chunk(current_chunk);
      // TODO: process validity buffer too
      auto chunk_data = reinterpret_cast<T *>(chunk.data()->buffers[1]->mutable_data());
      for (; new_chunk_pos < chunk_size && current_chunk_pos < chunk.length();
           current_chunk_pos++, total_elements_pos++) {
        // TODO: this wouldn't vectorize with division.
        // Need to rewrite it to for(int i=0; i<8; i++) or smth like that
        data_out[new_chunk_pos] = chunk_data[current_chunk_pos];
        new_chunk_pos += (bitmap[total_elements_pos / 8] >> (7 - (total_elements_pos % 8))) & 1;
      }
      if (current_chunk_pos == chunk.length()) {
        current_chunk++;
        current_chunk_pos = 0;
      }
    }
    auto array_data = arrow::ArrayData::Make(
        type, new_chunk_pos, {nullptr, std::shared_ptr<arrow::Buffer>(std::move(buffer))});
    auto array = arrow::MakeArray(array_data);
    new_column.push_back(array);
  } while (total_elements_pos < total_length);
  return std::make_shared<arrow::ChunkedArray>(new_column);
}

std::shared_ptr<ExecutionContext> materialize_filter(const std::shared_ptr<ExecutionContext> &ctx) {
  auto bitmap = ctx->metadata->filter_bitmap;
  if (!bitmap) {
    throw UnreachableException();
  }

  std::vector<std::shared_ptr<arrow::ChunkedArray>> new_columns;
  auto &table = *ctx->table;
  for (int col_num = 0; col_num < ctx->table->num_columns(); col_num++) {
    switch (table.column(col_num)->type()->id()) {
      PEFA_CASE_BRK(PEFA_INT8_CASE, new_columns.push_back(materialize_column<int8_t>(
                                        *table.column(col_num), bitmap->data())))
      PEFA_CASE_BRK(PEFA_INT16_CASE, new_columns.push_back(materialize_column<int16_t>(
                                         *table.column(col_num), bitmap->data())))
      PEFA_CASE_BRK(PEFA_INT32_CASE, new_columns.push_back(materialize_column<int32_t>(
                                         *table.column(col_num), bitmap->data())))
      PEFA_CASE_BRK(PEFA_INT64_CASE, new_columns.push_back(materialize_column<int64_t>(
                                         *table.column(col_num), bitmap->data())))
      PEFA_CASE_BRK(PEFA_UINT8_CASE, new_columns.push_back(materialize_column<uint8_t>(
                                         *table.column(col_num), bitmap->data())))
      PEFA_CASE_BRK(PEFA_UINT16_CASE, new_columns.push_back(materialize_column<uint16_t>(
                                          *table.column(col_num), bitmap->data())))
      PEFA_CASE_BRK(PEFA_UINT32_CASE, new_columns.push_back(materialize_column<uint32_t>(
                                          *table.column(col_num), bitmap->data())))
      PEFA_CASE_BRK(PEFA_UINT64_CASE, new_columns.push_back(materialize_column<uint64_t>(
                                          *table.column(col_num), bitmap->data())))
      PEFA_CASE_BRK(PEFA_FLOAT32_CASE, new_columns.push_back(materialize_column<float>(
                                           *table.column(col_num), bitmap->data())))
      PEFA_CASE_BRK(PEFA_FLOAT64_CASE, new_columns.push_back(materialize_column<double>(
                                           *table.column(col_num), bitmap->data())))
    default:
      throw NotImplementedException("Type " + table.column(col_num)->type()->ToString() +
                                    " is not supported yet");
    }
  }
  return std::make_shared<ExecutionContext>(arrow::Table::Make(ctx->table->schema(), new_columns));
}

template <typename T>
void merge_aggregates(std::unordered_map<T, uint64_t> &lhs,
                      const std::unordered_map<T, uint64_t> &rhs) {
  for (auto el : rhs) {
    if (lhs.count(el.first)) {
      lhs[el.first] += el.second;
    } else {
      lhs[el.first] = el.second;
    }
  }
}

template <typename T>
std::unordered_map<T, uint64_t> aggregate_single_chunk(const std::shared_ptr<ExecutionContext> &ctx,
                                                       const arrow::Array &array, size_t offset) {
  std::unordered_map<T, uint64_t> result;
  auto data = reinterpret_cast<const T *>(array.data()->buffers[1]->data());
  auto len = array.length();
  if (ctx->metadata->filter_bitmap) {
    for (int i = 0; i < len; i++) {
      if ((ctx->metadata->filter_bitmap->data()[(offset + i) / 8] >> (7 - (offset + i) % 8)) & 1) {
        if (result.count(data[i])) {
          result[data[i]]++;
        } else {
          result[data[i]] = 1;
        }
      }
    }
  } else {
    for (int i = 0; i < len; i++) {
      if (result.count(data[i])) {
        result[data[i]]++;
      } else {
        result[data[i]] = 1;
      }
    }
  }
  return result;
}

template <typename T>
std::unordered_map<T, uint64_t>
aggregate_single_column(const std::shared_ptr<ExecutionContext> &ctx,
                        const arrow::ChunkedArray &column) {
  std::unordered_map<T, uint64_t> result;
  size_t offset = 0;
  for (auto chunk : column.chunks()) {
    merge_aggregates<T>(result, aggregate_single_chunk<T>(ctx, *chunk, offset));
    offset += chunk->length();
  }
  return result;
}

template <typename T>
std::shared_ptr<ExecutionContext> typed_aggregate(const std::shared_ptr<ExecutionContext> &ctx,
                                                  arrow::ChunkedArray &column,
                                                  std::shared_ptr<arrow::Field> field) {
  using arrow_type = typename arrow::CTypeTraits<T>::ArrowType;
  auto map = aggregate_single_column<T>(ctx, column);
  auto schema = std::make_shared<arrow::Schema>(
      std::vector{field, std::make_shared<arrow::Field>("count", arrow::uint64())});

  std::unique_ptr<arrow::ArrayBuilder> values_builder;
  std::unique_ptr<arrow::ArrayBuilder> counts_builder;
  arrow::MakeBuilder(arrow::default_memory_pool(), field->type(), &values_builder);
  arrow::MakeBuilder(arrow::default_memory_pool(), arrow::uint64(), &counts_builder);
  values_builder->Reserve(map.size());
  counts_builder->Reserve(map.size());

  for (auto el : map) {
    static_cast<arrow::NumericBuilder<arrow_type> &>(*values_builder).Append(el.first);
    static_cast<arrow::NumericBuilder<arrow::UInt64Type> &>(*counts_builder).Append(el.second);
  }
  std::shared_ptr<arrow::Array> values_column;
  std::shared_ptr<arrow::Array> counts_column;
  values_builder->Finish(&values_column);
  counts_builder->Finish(&counts_column);
  return std::make_shared<ExecutionContext>(
      arrow::Table::Make(schema, {std::make_shared<arrow::ChunkedArray>(values_column),
                                  std::make_shared<arrow::ChunkedArray>(counts_column)}));
}

std::shared_ptr<ExecutionContext> group_by(const std::shared_ptr<ExecutionContext> &ctx,
                                           std::vector<std::string> columns) {
  auto col = ctx->table->GetColumnByName(columns[0]);
  auto field = ctx->table->schema()->GetFieldByName(columns[0]);
  switch (col->type()->id()) {
    PEFA_CASE_RET(PEFA_INT8_CASE, typed_aggregate<int8_t>(ctx, *col, field))
    PEFA_CASE_RET(PEFA_INT16_CASE, typed_aggregate<int16_t>(ctx, *col, field))
    PEFA_CASE_RET(PEFA_INT32_CASE, typed_aggregate<int32_t>(ctx, *col, field))
    PEFA_CASE_RET(PEFA_INT64_CASE, typed_aggregate<int64_t>(ctx, *col, field))
    PEFA_CASE_RET(PEFA_UINT8_CASE, typed_aggregate<uint8_t>(ctx, *col, field))
    PEFA_CASE_RET(PEFA_UINT16_CASE, typed_aggregate<uint16_t>(ctx, *col, field))
    PEFA_CASE_RET(PEFA_UINT32_CASE, typed_aggregate<uint32_t>(ctx, *col, field))
    PEFA_CASE_RET(PEFA_UINT64_CASE, typed_aggregate<uint64_t>(ctx, *col, field))
    PEFA_CASE_RET(PEFA_FLOAT32_CASE, typed_aggregate<float>(ctx, *col, field))
    PEFA_CASE_RET(PEFA_FLOAT64_CASE, typed_aggregate<double>(ctx, *col, field))
  default:
    throw NotImplementedException("group by " + col->type()->name() + " is not implemented");
  }
}
} // namespace pefa::execution

#include "backend.h"

#include "pefa/kernels/filter.h"

#include <memory>

namespace pefa::backends::naive {
using namespace internal;
std::shared_ptr<Context> Backend::project(const std::shared_ptr<Context> ctx,
                                          std::vector<std::string> column_names) {
  std::vector<std::shared_ptr<arrow::ChunkedArray>> columns(column_names.size());
  std::vector<std::shared_ptr<arrow::Field>> fields(column_names.size());
  for (int i = 0; i < column_names.size(); i++) {
    auto col = ctx->table->GetColumnByName(column_names[i]);
    fields[i] = std::make_shared<arrow::Field>(column_names[i], col->type());
    columns[i] = col;
  }
  return std::make_shared<Context>(
      arrow::Table::Make(std::make_shared<arrow::Schema>(fields), columns));
}

std::shared_ptr<arrow::Table> Backend::execute(const std::shared_ptr<Context> ctx) {
  return ctx->table;
}
std::shared_ptr<Context> Backend::filter(const std::shared_ptr<Context> ctx,
                                         const std::shared_ptr<internal::Expr> expr) {
  // TODO: make that implementation parallel
  auto schema = ctx->table->schema();
  auto fields_count = schema->num_fields();
  if (fields_count == 0 || ctx->table->column(0)->num_chunks() == 0) {
    // TODO: make Context const and do a copy
    return ctx;
  }
  std::vector<std::unique_ptr<kernels::FilterKernel>> kernels(fields_count);
  for (int i = 0; i < fields_count; i++) {
    kernels[i] = kernels::FilterKernel::create_cpu(schema->field(i), expr);
    kernels[i]->compile();
  }
  std::vector<std::shared_ptr<arrow::ChunkedArray>> columns(fields_count);
  std::vector<std::shared_ptr<arrow::Buffer>> column_bitmap(fields_count);

  for (int col_num = 0; col_num < fields_count; col_num++) {
    auto column = ctx->table->column(col_num);
    auto filter_bitmap = arrow::AllocateEmptyBitmap(column->length()).ValueOrDie();
    column_bitmap[col_num] = filter_bitmap;
    for (int chunk_num = 0; chunk_num < column->num_chunks(); chunk_num++) {
      size_t offset = 0;
      for (int i = 0; i < chunk_num; i++) {
        offset += column->chunk(i)->length();
      }
      kernels[col_num]->execute(column->chunk(chunk_num),
                                filter_bitmap->mutable_data() + offset / 8, offset % 8);
    }
  }

  for (int col_num = 0; col_num < fields_count; col_num++) {
    size_t offset = 0;
    auto column = ctx->table->column(col_num);
    auto filter_bitmap = column_bitmap[col_num];
    for (int chunk_num = 0; chunk_num < column->num_chunks(); chunk_num++) {
      if (offset % 8 != 0) {
        kernels[col_num]->execute_remaining(
            column->chunk(chunk_num), filter_bitmap->mutable_data() + offset / 8, 0, offset % 2);
      }
      offset += column->chunk(chunk_num)->length();
      if (offset % 8 != 0) {
        kernels[col_num]->execute_remaining(column->chunk(chunk_num),
                                            filter_bitmap->mutable_data() + offset / 8,
                                            column->chunk(chunk_num)->length() - (offset % 8), 0);
      }
    }
  }

  auto result_buf = column_bitmap[0]->mutable_data();
  for (int i = 1; i < fields_count; i++) {
    auto current_buf = column_bitmap[i]->data();
    for (int j = 0; j < column_bitmap[0]->size(); j++) {
      result_buf[j] &= current_buf[j];
    }
  }
  
  // TODO: apply filter to all columns
  // As a solution for this we can calculate length of all arrays by using popcnt
  // Then allocate blocks with that lengths (merge small blocks if any)
  // And create chunks in parallel
  // Very big chunks can be split later by slicing
  return std::make_shared<Context>(ctx->table); // TODO: return generated table context
}

} // namespace pefa::backends::naive

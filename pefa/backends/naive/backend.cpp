#include "backend.h"

#include "pefa/kernels/filter.h"

#include <memory>

namespace pefa::backends::naive {
using namespace internal;
std::shared_ptr<Context> Backend::project(std::shared_ptr<Context> ctx, std::vector<std::string> column_names) {
  std::vector<std::shared_ptr<arrow::ChunkedArray>> columns(column_names.size());
  std::vector<std::shared_ptr<arrow::Field>> fields(column_names.size());
  for (int i = 0; i < column_names.size(); i++) {
    auto col = ctx->table->GetColumnByName(column_names[i]);
    fields[i] = std::make_shared<arrow::Field>(column_names[i], col->type());
    columns[i] = col;
  }
  return std::make_shared<Context>(arrow::Table::Make(std::make_shared<arrow::Schema>(fields), columns));
}

std::shared_ptr<arrow::Table> Backend::execute(std::shared_ptr<Context> ctx) {
  return ctx->table;
}
std::shared_ptr<Context> Backend::filter(std::shared_ptr<Context> ctx, std::shared_ptr<internal::Expr> expr) {
  // TODO: make that implementation parallel
  auto schema = ctx->table->schema();
  auto fields_count = schema->num_fields();
  if (fields_count == 0 || ctx->table->column(0)->num_chunks() == 0) {
    return ctx;
  }
  std::vector<std::unique_ptr<kernels::FilterKernel>> kernels(fields_count);
  for (int i = 0; i < fields_count; i++) {
    kernels[i] = kernels::FilterKernel::create_cpu(schema->field(i), expr);
  }
  std::vector<std::shared_ptr<arrow::ChunkedArray>> columns(fields_count);
  std::vector<std::shared_ptr<arrow::Buffer>> column_bitmap(fields_count);

  for (int col_num = 0; col_num < fields_count; col_num++) {
    auto original_column = ctx->table->column(col_num);
    std::vector<std::shared_ptr<arrow::Array>> chunks(original_column->num_chunks());
    auto filter_bitmap = arrow::AllocateEmptyBitmap(original_column->length()).ValueOrDie();
    column_bitmap[col_num] = filter_bitmap;
    for (int chunk_num = 0; chunk_num < original_column->num_chunks(); chunk_num++) {
      size_t offset = 0;
      for (int i = 0; i < chunk_num; i++) {
        offset += original_column->chunk(i)->length();
      }
      kernels[col_num]->execute(original_column->chunk(chunk_num), filter_bitmap->mutable_data() + offset / 8, offset % 8);
    }
  }
  // TODO: process skipped by filter bitmaps
  // we don't process unaligned elements (e.g last and first) in each chunk
  // we do need extra pass to process them
  // unaligned means elements from different chunk, that should share the same bitmap
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
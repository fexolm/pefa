#include "backend.h"

#include <memory>

namespace pefa::backends::naive {

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

} // namespace pefa::backends::naive
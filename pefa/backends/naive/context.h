#pragma once
#include <arrow/table.h>
#include <utility>

namespace pefa::backends::naive {
struct Context {
  std::shared_ptr<arrow::Table> table;
  explicit Context(std::shared_ptr<arrow::Table> table)
      : table(std::move(table)) {}
};
} // namespace pefa::backends::naive

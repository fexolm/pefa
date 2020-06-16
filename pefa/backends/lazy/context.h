#pragma once
#include "logical_plan.h"

#include <arrow/table.h>
#include <memory>

namespace pefa::backends::lazy {
struct Context {
  std::shared_ptr<arrow::Table> table;
  std::shared_ptr<LogicalPlan> plan;
  explicit Context(std::shared_ptr<arrow::Table> table, std::shared_ptr<LogicalPlan> plan = nullptr)
      : table(std::move(table))
      , plan(std::move(plan)) {}
};
} // namespace pefa::backends::lazy
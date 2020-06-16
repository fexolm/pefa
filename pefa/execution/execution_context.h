#pragma once
#include "pefa/query_compiler/logical_plan.h"

#include <arrow/table.h>
#include <memory>

namespace pefa::execution {
using namespace query_compiler;

struct ExecutionContext {
  std::shared_ptr<arrow::Table> table;
  std::shared_ptr<LogicalPlan> plan;
  explicit ExecutionContext(std::shared_ptr<arrow::Table> table,
                            std::shared_ptr<LogicalPlan> plan = nullptr)
      : table(std::move(table))
      , plan(std::move(plan)) {}
};
} // namespace pefa::execution
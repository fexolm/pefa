#pragma once
#include "expressions.h"
#include "logical_plan.h"

#include <arrow/table.h>
#include <memory>
#include <utility>

namespace pefa::query_compiler {
class QueryCompiler {
private:
  std::shared_ptr<LogicalPlan> m_plan;

  explicit QueryCompiler(std::shared_ptr<LogicalPlan> plan);

public:
  explicit QueryCompiler();

  [[nodiscard]] QueryCompiler project(const std::vector<std::string> &columns) const;

  [[nodiscard]] QueryCompiler filter(std::shared_ptr<BooleanExpr> expr) const;

  [[nodiscard]] std::shared_ptr<arrow::Table>
  execute(const std::shared_ptr<arrow::Table> &table) const;
};
} // namespace pefa::query_compiler
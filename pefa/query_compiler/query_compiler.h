#pragma once
#include "expressions.h"
#include "pefa/execution/execution_context.h"

#include <arrow/table.h>
#include <memory>
#include <utility>

namespace pefa::query_compiler {
using execution::ExecutionContext;
class QueryCompiler {
private:
  std::shared_ptr<ExecutionContext> m_ctx;

  explicit QueryCompiler(std::shared_ptr<ExecutionContext> ctx);

public:
  explicit QueryCompiler(std::shared_ptr<arrow::Table> table);

  [[nodiscard]] QueryCompiler project(const std::vector<std::string> &columns) const;

  [[nodiscard]] QueryCompiler filter(std::shared_ptr<BooleanExpr> expr) const;

  [[nodiscard]] std::shared_ptr<arrow::Table> execute() const;
};
} // namespace pefa::query_compiler
#pragma once
#include "execution_context.h"
#include "pefa/query_compiler/expressions.h"

namespace pefa::execution {
using namespace query_compiler;
[[nodiscard]] std::shared_ptr<ExecutionContext>
project(const std::shared_ptr<ExecutionContext> &ctx, std::vector<std::string> columns);

[[nodiscard]] std::shared_ptr<ExecutionContext> filter(const std::shared_ptr<ExecutionContext> &ctx,
                                                       const std::shared_ptr<BooleanExpr> &expr);
} // namespace pefa::execution

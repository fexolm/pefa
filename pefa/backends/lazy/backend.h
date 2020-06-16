#pragma once
#include "context.h"
#include "pefa/api/expressions.h"

#include <arrow/table.h>
#include <memory>
namespace pefa::backends::lazy {
class Backend {
  typedef Context ContextType;

  static std::shared_ptr<Context> project(const std::shared_ptr<Context> &ctx,
                                          const std::vector<std::string> &columns);

  static std::shared_ptr<Context> filter(const std::shared_ptr<Context> &ctx,
                                         const std::shared_ptr<internal::Expr> &expr);

  static std::shared_ptr<arrow::Table> execute(const std::shared_ptr<Context> &ctx);
};

} // namespace pefa::backends::lazy

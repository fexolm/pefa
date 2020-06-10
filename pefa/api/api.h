#pragma once
#include "expressions.h"
#include "pefa/backends/backend_traits.h"

#include <arrow/table.h>
#include <memory>

namespace pefa {
template <typename Backend> class ExecutionContext {
private:
  using Context = typename backends::BackendTraits<Backend>::ContextType;
  std::shared_ptr<Context> m_backendCtx;

  explicit ExecutionContext(std::shared_ptr<Context> ctx)
      : m_backendCtx(ctx) {}

public:
  explicit ExecutionContext(std::shared_ptr<arrow::Table> table)
      : m_backendCtx(std::make_shared<Context>(table)) {}

  ExecutionContext project(std::vector<std::string> columns) const {
    return ExecutionContext(Backend::project(m_backendCtx, std::move(columns)));
  }

  ExecutionContext filter(std::shared_ptr<internal::Expr> expr) const {
    return ExecutionContext(Backend::filter(m_backendCtx, std::move(expr)));
  }

  std::shared_ptr<arrow::Table> execute() const {
    return Backend::execute(m_backendCtx);
  }
};
} // namespace pefa
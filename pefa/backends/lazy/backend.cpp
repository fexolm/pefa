#include "backend.h"

#include "pefa/backends/naive/backend.h"
#include "pefa/backends/naive/context.h"

#include <memory>
namespace pefa::backends::lazy {
std::shared_ptr<Context> Backend::project(const std::shared_ptr<Context> &ctx,
                                          const std::vector<std::string> &columns) {
  return std::make_shared<Context>(ctx->table,
                                   std::make_shared<ProjectionNode>(ctx->plan, columns));
}
std::shared_ptr<Context> Backend::filter(const std::shared_ptr<Context> &ctx,
                                         const std::shared_ptr<internal::Expr> &expr) {
  return std::shared_ptr<Context>();
}

struct ExecutePlanVisitor : PlanVisitor {
  std::shared_ptr<naive::Context> ctx;
  explicit ExecutePlanVisitor(std::shared_ptr<naive::Context> ctx)
      : ctx(std::move(ctx)) {}
  void visit(const ProjectionNode &node) override {
    if (node.input) {
      node.input->visit(*this);
    }
    ctx = naive::Backend::project(ctx, node.fields);
  }
  void visit(const FilterNode &node) override {
    if (node.input) {
      node.input->visit(*this);
    }
    ctx = naive::Backend::filter(ctx, node.expr);
  }
};

std::shared_ptr<arrow::Table> Backend::execute(const std::shared_ptr<Context> &ctx) {
  auto naive_ctx = std::make_shared<naive::Context>(ctx->table);
  auto visitor = ExecutePlanVisitor(naive_ctx);
  ctx->plan->visit(visitor);
  return visitor.ctx->table;
}
} // namespace pefa::backends::lazy

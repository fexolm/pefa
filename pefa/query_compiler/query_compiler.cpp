#include "query_compiler.h"

#include "pefa/execution/execution.h"
#include "plan_optimizer.h"

namespace pefa::query_compiler {

QueryCompiler::QueryCompiler(std::shared_ptr<ExecutionContext> ctx)
    : m_ctx(std::move(ctx)) {}

QueryCompiler::QueryCompiler(std::shared_ptr<arrow::Table> table)
    : m_ctx(std::make_shared<ExecutionContext>(std::move(table))) {}

QueryCompiler QueryCompiler::project(const std::vector<std::string> &columns) const {
  return QueryCompiler(std::make_shared<ExecutionContext>(
      m_ctx->table, std::make_shared<ProjectionNode>(m_ctx->plan, columns)));
}

QueryCompiler QueryCompiler::filter(std::shared_ptr<BooleanExpr> expr) const {
  return QueryCompiler(std::make_shared<ExecutionContext>(
      m_ctx->table, std::make_shared<FilterNode>(m_ctx->plan, std::move(expr))));
}

struct ExecutePlanVisitor : PlanVisitor {
  std::shared_ptr<ExecutionContext> ctx;

  explicit ExecutePlanVisitor(std::shared_ptr<ExecutionContext> ctx)
      : ctx(std::move(ctx)) {}

  void visit(const ProjectionNode &node) override {
    PlanVisitor::visit(node);
    ctx = execution::project(ctx, node.fields);
  }

  void visit(const FilterNode &node) override {
    PlanVisitor::visit(node);
    ctx = execution::filter(ctx, node.expr);
  }
};

std::shared_ptr<arrow::Table> QueryCompiler::execute() const {
  PlanOptimizer optimizer;
  optimizer.add_pass(JoinFilterPass::create());
  auto plan = optimizer.run(m_ctx->plan);

  auto ctx = std::make_shared<ExecutionContext>(m_ctx->table, nullptr);

  auto visitor = ExecutePlanVisitor(ctx);
  plan->visit(visitor);
  return visitor.ctx->table;
}
} // namespace pefa::query_compiler
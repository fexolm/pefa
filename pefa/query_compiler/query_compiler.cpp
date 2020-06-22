#include "query_compiler.h"

#include "pefa/execution/execution.h"
#include "pefa/execution/execution_context.h"
#include "pefa/query_compiler/lp_optimizer/join_filter_pass.h"
#include "pefa/query_compiler/lp_optimizer/plan_optimizer.h"

#include <utility>

namespace pefa::query_compiler {

QueryCompiler::QueryCompiler()
    : m_plan(nullptr) {}

QueryCompiler::QueryCompiler(std::shared_ptr<LogicalPlan> plan)
    : m_plan(std::move(plan)) {}

QueryCompiler QueryCompiler::project(const std::vector<std::string> &columns) const {
  return QueryCompiler(std::make_shared<ProjectionNode>(m_plan, columns));
}

QueryCompiler QueryCompiler::filter(std::shared_ptr<BooleanExpr> expr) const {
  return QueryCompiler(std::make_shared<MaterializeFilterNode>(
      std::make_shared<FilterNode>(m_plan, std::move(expr))));
}

struct ExecutePlanVisitor : PlanVisitor {
  std::shared_ptr<execution::ExecutionContext> ctx;

  explicit ExecutePlanVisitor(std::shared_ptr<execution::ExecutionContext> ctx)
      : ctx(std::move(ctx)) {}

  void on_visit(const ProjectionNode &node) override {
    ctx = execution::project(ctx, node.fields);
  }

  void on_visit(const FilterNode &node) override {
    ctx = execution::generate_filter_bitmap(ctx, node.expr);
  }

  void on_visit(const MaterializeFilterNode &node) override {
    ctx = execution::materialize_filter(ctx);
  }
};

std::shared_ptr<arrow::Table>
QueryCompiler::execute(const std::shared_ptr<arrow::Table> &table) const {
  PlanOptimizer optimizer;
  optimizer.add_pass(JoinFilterPass::create());
  auto plan = optimizer.run(m_plan);

  auto ctx = std::make_shared<execution::ExecutionContext>(table);

  auto visitor = ExecutePlanVisitor(ctx);
  plan->visit(visitor);
  return visitor.ctx->table;
}
} // namespace pefa::query_compiler
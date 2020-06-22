#include "plan_optimizer.h"
namespace pefa::query_compiler {
void PlanOptimizer::add_pass(std::unique_ptr<OptimizerPass> pass) {
  m_passes.emplace_back(std::move(pass));
}
std::shared_ptr<LogicalPlan> PlanOptimizer::run(std::shared_ptr<LogicalPlan> input) {
  auto res = std::move(input);
  for (auto &pass : m_passes) {
    res = pass->execute(res);
  }
  return res;
}

void OptimizerPass::on_visit(const FilterNode &node) {
  m_result = std::make_shared<FilterNode>(m_result, node.expr);
}

void OptimizerPass::on_visit(const MaterializeFilterNode &node) {
  m_result = std::make_shared<MaterializeFilterNode>(m_result);
}

void OptimizerPass::on_visit(const ProjectionNode &node) {
  m_result = std::make_shared<ProjectionNode>(m_result, node.fields);
}

std::shared_ptr<LogicalPlan> OptimizerPass::execute(const std::shared_ptr<LogicalPlan> &input) {
  input->visit(*this);
  return m_result;
}
} // namespace pefa::query_compiler

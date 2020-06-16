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
} // namespace pefa::query_compiler

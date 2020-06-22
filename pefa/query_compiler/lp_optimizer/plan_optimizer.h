#pragma once
#include "pefa/query_compiler/logical_plan.h"

#include <memory>
#include <utility>

namespace pefa::query_compiler {

class OptimizerPass : public PlanVisitor {
protected:
  std::shared_ptr<LogicalPlan> m_result;

public:
  [[nodiscard]] virtual std::shared_ptr<LogicalPlan>
  execute(const std::shared_ptr<LogicalPlan> &input);

  virtual ~OptimizerPass() = default;

  void on_visit(const FilterNode &node) override;
  void on_visit(const MaterializeFilterNode &node) override;
  void on_visit(const ProjectionNode &node) override;
};

class PlanOptimizer {
private:
  std::vector<std::unique_ptr<OptimizerPass>> m_passes{};

public:
  PlanOptimizer() = default;

  void add_pass(std::unique_ptr<OptimizerPass> pass);

  [[nodiscard]] std::shared_ptr<LogicalPlan> run(std::shared_ptr<LogicalPlan> input);
};
} // namespace pefa::query_compiler
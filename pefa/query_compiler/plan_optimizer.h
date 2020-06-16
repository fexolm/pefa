#pragma once
#include "logical_plan.h"

#include <memory>
#include <utility>

namespace pefa::query_compiler {

struct OptimizerPass : PlanVisitor {
  [[nodiscard]] virtual std::shared_ptr<LogicalPlan>
  execute(std::shared_ptr<LogicalPlan> input) = 0;
  virtual ~OptimizerPass() = default;
};

class JoinFilterPass : public OptimizerPass {
private:
  std::shared_ptr<LogicalPlan> result{nullptr};

public:
  JoinFilterPass() = default;

  [[nodiscard]] std::shared_ptr<LogicalPlan> execute(std::shared_ptr<LogicalPlan> input) override {
    input->visit(*this);
    return result;
  }

  void visit(const ProjectionNode &node) override {
    OptimizerPass::visit(node);
    result = std::make_shared<ProjectionNode>(result, node.fields);
  }

  void visit(const FilterNode &node) override {
    OptimizerPass::visit(node);
    if (auto prev = std::dynamic_pointer_cast<FilterNode>(result)) {
      prev->expr = prev->expr->AND(node.expr);
    } else {
      result = std::make_shared<FilterNode>(result, node.expr);
    }
  }

  [[nodiscard]] static std::unique_ptr<JoinFilterPass> create() {
    return std::make_unique<JoinFilterPass>();
  }
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
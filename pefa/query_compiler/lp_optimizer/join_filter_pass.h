#pragma once
#include "pefa/query_compiler/logical_plan.h"
#include "plan_optimizer.h"

namespace pefa::query_compiler {
class JoinFilterPass : public OptimizerPass {
public:
  JoinFilterPass() = default;

  void on_visit(const FilterNode &node) override;

  [[nodiscard]] static std::unique_ptr<JoinFilterPass> create();
};
} // namespace pefa::query_compiler
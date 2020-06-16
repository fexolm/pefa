#pragma once
#include "expressions.h"

#include <memory>
#include <vector>

namespace pefa::query_compiler {
struct PlanVisitor;

struct LogicalPlan : std::enable_shared_from_this<LogicalPlan> {
  virtual void visit(PlanVisitor &visitor) const = 0;
};

struct ProjectionNode : LogicalPlan {
  std::shared_ptr<LogicalPlan> input;
  std::vector<std::string> fields;
  void visit(PlanVisitor &visitor) const override;
  ProjectionNode(std::shared_ptr<LogicalPlan> input, std::vector<std::string> fields);
};

struct FilterNode : LogicalPlan {
  std::shared_ptr<LogicalPlan> input;
  std::shared_ptr<BooleanExpr> expr;
  void visit(PlanVisitor &visitor) const override;
  FilterNode(std::shared_ptr<LogicalPlan> input, std::shared_ptr<BooleanExpr> expr);
};

struct PlanVisitor {
  virtual void visit(const ProjectionNode &node);
  virtual void visit(const FilterNode &node);
};

} // namespace pefa::query_compiler
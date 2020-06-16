#pragma once
#include "pefa/api/expressions.h"

#include <memory>
#include <vector>

namespace pefa::backends::lazy {
struct PlanVisitor;

struct LogicalPlan : std::enable_shared_from_this<LogicalPlan> {
  virtual void visit(PlanVisitor &visitor) const = 0;
};

struct ProjectionNode : LogicalPlan {
  std::shared_ptr<const LogicalPlan> input;
  std::vector<std::string> fields;
  void visit(PlanVisitor &visitor) const override;
  ProjectionNode(std::shared_ptr<const LogicalPlan> input, std::vector<std::string> fields);
};

struct FilterNode : LogicalPlan {
  std::shared_ptr<const LogicalPlan> input;
  std::shared_ptr<internal::Expr> expr;
  void visit(PlanVisitor &visitor) const override;
  FilterNode(std::shared_ptr<const LogicalPlan> input, std::shared_ptr<internal::Expr> expr);
};

struct PlanVisitor {
  virtual void visit(const ProjectionNode &node) = 0;
  virtual void visit(const FilterNode &node) = 0;
};

} // namespace pefa::backends::lazy
#pragma once
#include "expressions.h"

#include <memory>
#include <vector>

namespace pefa::query_compiler {
class PlanVisitor;

struct LogicalPlan : std::enable_shared_from_this<LogicalPlan> {
  virtual void visit(PlanVisitor &visitor) const = 0;
};

struct ProjectionNode : LogicalPlan {
  std::shared_ptr<LogicalPlan> input;
  std::vector<std::string> fields;
  void visit(PlanVisitor &visitor) const override;
  ProjectionNode(std::shared_ptr<LogicalPlan> input, std::vector<std::string> fields);
};

struct MaterializeFilterNode : LogicalPlan {
  std::shared_ptr<LogicalPlan> input;
  void visit(PlanVisitor &visitor) const override;
  explicit MaterializeFilterNode(std::shared_ptr<LogicalPlan> input);
};

struct FilterNode : LogicalPlan {
  std::shared_ptr<LogicalPlan> input;
  std::shared_ptr<BooleanExpr> expr;
  void visit(PlanVisitor &visitor) const override;
  FilterNode(std::shared_ptr<LogicalPlan> input, std::shared_ptr<BooleanExpr> expr);
};

class PlanVisitor {
public:
  void visit(const ProjectionNode &node);
  void visit(const FilterNode &node);
  void visit(const MaterializeFilterNode &node);

protected:
  virtual void on_visit(const ProjectionNode &node) = 0;
  virtual void on_visit(const FilterNode &node) = 0;
  virtual void on_visit(const MaterializeFilterNode &node) = 0;
};

} // namespace pefa::query_compiler
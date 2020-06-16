#include "logical_plan.h"

namespace pefa::backends::lazy {
void ProjectionNode::visit(PlanVisitor &visitor) const {
  visitor.visit(*this);
}
void FilterNode::visit(PlanVisitor &visitor) const {
  visitor.visit(*this);
}
ProjectionNode::ProjectionNode(std::shared_ptr<LogicalPlan> input,
                               std::vector<std::string> fields)
    : input(std::move(input))
    , fields(std::move(fields)) {}
FilterNode::FilterNode(std::shared_ptr<LogicalPlan> input,
                       std::shared_ptr<internal::BooleanExpr> expr)
    : input(std::move(input))
    , expr(std::move(expr)) {}

void PlanVisitor::visit(const ProjectionNode &node) {
  if (node.input) {
    node.input->visit(*this);
  }
}
void PlanVisitor::visit(const FilterNode &node) {
  if (node.input) {
    node.input->visit(*this);
  }
}
} // namespace pefa::backends::lazy
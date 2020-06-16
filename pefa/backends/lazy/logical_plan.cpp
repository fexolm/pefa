#include "logical_plan.h"

namespace pefa::backends::lazy {
void ProjectionNode::visit(PlanVisitor &visitor) const {
  visitor.visit(*this);
}
void FilterNode::visit(PlanVisitor &visitor) const {
  visitor.visit(*this);
}
ProjectionNode::ProjectionNode(std::shared_ptr<const LogicalPlan> input,
                               std::vector<std::string> fields)
    : input(std::move(input))
    , fields(std::move(fields)) {}
FilterNode::FilterNode(std::shared_ptr<const LogicalPlan> input,
                       std::shared_ptr<internal::Expr> expr)
    : input(std::move(input))
    , expr(std::move(expr)) {}
} // namespace pefa::backends::lazy
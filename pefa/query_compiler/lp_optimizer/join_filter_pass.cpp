#include "join_filter_pass.h"
namespace pefa::query_compiler {

void JoinFilterPass::on_visit(const FilterNode &node) {
  if (auto prev = std::dynamic_pointer_cast<FilterNode>(m_result)) {
    prev->expr = prev->expr->AND(node.expr);
  } else if (auto prev = std::dynamic_pointer_cast<MaterializeFilterNode>(m_result)) { // NOLINT
    // remove unnecessary materialization and revisit
    // TODO: also make materialization push down optimizer
    m_result = prev->input;
    on_visit(node);
    return;
  } else {
    m_result = std::make_shared<FilterNode>(m_result, node.expr);
  }
}

std::unique_ptr<JoinFilterPass> JoinFilterPass::create() {
  return std::make_unique<JoinFilterPass>();
}
} // namespace pefa::query_compiler
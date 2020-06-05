#pragma once
#include "expressions.h"

#include <arrow/api.h>
#include <memory>
#include <stack>
namespace pefa::internal {

class SplitExprVisitor : public ExprVisitor {
private:
  std::shared_ptr<BooleanExpr> m_result;
  std::string m_field_name;

public:
  explicit SplitExprVisitor(std::string field_name) : m_field_name(field_name) {}

  void visit(const PredicateExpr &expr) override {
    expr.lhs->visit(*this);
    auto lhs = m_result;
    expr.rhs->visit(*this);
    auto rhs = m_result;
    m_result = PredicateExpr::create(lhs, rhs, expr.op);
  }

  void visit(const CompareExpr &expr) override {
    if (expr.lhs->name == m_field_name) {
      m_result =
          std::static_pointer_cast<BooleanExpr>(const_cast<CompareExpr &>(expr).shared_from_this());
    } else {
      m_result = BooleanConst::create(true);
    }
  }

  std::shared_ptr<Expr> expr() {
    return m_result;
  }
};

std::vector<std::shared_ptr<Expr>> split_expr_to_columns(std::shared_ptr<arrow::Schema> schema,
                                                         std::shared_ptr<Expr> expr) {
  std::vector<std::shared_ptr<Expr>> result(schema->num_fields());
  for (int i = 0; i < schema->num_fields(); i++) {
    SplitExprVisitor visitor(schema->field(i)->name());
    expr->visit(visitor);
    result[i] = visitor.expr();
  }
  return result;
}



} // namespace pefa::internal
#include "expressions.h"
namespace pefa::internal {

ColumnRef::ColumnRef(std::string name)
    : name(std::move(name)) {}

std::shared_ptr<ColumnRef> ColumnRef::create(std::string name) {
  return std::make_shared<ColumnRef>(std::move(name));
}
void ColumnRef::visit(ExprVisitor &visitor) {
  visitor.visit(*this);
}

std::shared_ptr<BooleanExpr> BooleanExpr::AND(std::shared_ptr<BooleanExpr> rhs) {
  return PredicateExpr::create(std::static_pointer_cast<BooleanExpr>(shared_from_this()), rhs,
                               PredicateExpr::Op::AND);
}

std::shared_ptr<BooleanExpr> BooleanExpr::OR(std::shared_ptr<BooleanExpr> rhs) {
  return PredicateExpr::create(std::static_pointer_cast<BooleanExpr>(shared_from_this()), rhs,
                               PredicateExpr::Op::OR);
}

BooleanConst::BooleanConst(bool val)
    : value(val) {}
std::shared_ptr<BooleanConst> BooleanConst::create(bool value) {
  return std::make_shared<BooleanConst>(value);
}
void BooleanConst::visit(ExprVisitor &visitor) {
  visitor.visit(*this);
}

std::shared_ptr<PredicateExpr> PredicateExpr::create(std::shared_ptr<BooleanExpr> lhs,
                                                     std::shared_ptr<BooleanExpr> rhs, Op op) {
  return std::make_shared<PredicateExpr>(std::move(lhs), std::move(rhs), op);
}

PredicateExpr::PredicateExpr(std::shared_ptr<BooleanExpr> lhs, std::shared_ptr<BooleanExpr> rhs,
                             Op op)
    : lhs(lhs)
    , rhs(rhs)
    , op(op) {}

void PredicateExpr::visit(ExprVisitor &visitor) {
  visitor.visit(*this);
}

CompareExpr::CompareExpr(std::shared_ptr<ColumnRef> lhs, std::shared_ptr<LiteralExpr> rhs, Op op)
    : lhs(lhs)
    , rhs(rhs)
    , op(op) {}

void CompareExpr::visit(ExprVisitor &visitor) {
  visitor.visit(*this);
}

std::shared_ptr<CompareExpr> CompareExpr::create(std::shared_ptr<ColumnRef> lhs,
                                                 std::shared_ptr<LiteralExpr> rhs, Op op) {
  return std::make_shared<CompareExpr>(std::move(lhs), std::move(rhs), op);
}

std::shared_ptr<CompareExpr> ColumnRef::EQ(std::shared_ptr<LiteralExpr> rhs) {
  return CompareExpr::create(std::static_pointer_cast<ColumnRef>(shared_from_this()), rhs,
                             CompareExpr::Op::EQ);
}
std::shared_ptr<CompareExpr> ColumnRef::LE(std::shared_ptr<LiteralExpr> rhs) {
  return CompareExpr::create(std::static_pointer_cast<ColumnRef>(shared_from_this()), rhs,
                             CompareExpr::Op::LE);
}
std::shared_ptr<CompareExpr> ColumnRef::GE(std::shared_ptr<LiteralExpr> rhs) {
  return CompareExpr::create(std::static_pointer_cast<ColumnRef>(shared_from_this()), rhs,
                             CompareExpr::Op::GE);
}
std::shared_ptr<CompareExpr> ColumnRef::NEQ(std::shared_ptr<LiteralExpr> rhs) {
  return CompareExpr::create(std::static_pointer_cast<ColumnRef>(shared_from_this()), rhs,
                             CompareExpr::Op::NEQ);
}
std::shared_ptr<CompareExpr> ColumnRef::LT(std::shared_ptr<LiteralExpr> rhs) {
  return CompareExpr::create(std::static_pointer_cast<ColumnRef>(shared_from_this()), rhs,
                             CompareExpr::Op::LT);
}
std::shared_ptr<CompareExpr> ColumnRef::GT(std::shared_ptr<LiteralExpr> rhs) {
  return CompareExpr::create(std::static_pointer_cast<ColumnRef>(shared_from_this()), rhs,
                             CompareExpr::Op::GT);
}
LiteralExpr::LiteralExpr(std::variant<int64_t, double, std::string, bool> val)
    : value(val) {}

void LiteralExpr::visit(ExprVisitor &visitor) {
  visitor.visit(*this);
}
std::shared_ptr<LiteralExpr>
LiteralExpr::create(std::variant<int64_t, double, std::string, bool> val) {
  return std::make_shared<LiteralExpr>(val);
}

void ExprVisitor::visit(const ColumnRef &expr) {}

void ExprVisitor::visit(const PredicateExpr &expr) {
  expr.lhs->visit(*this);
  expr.rhs->visit(*this);
}

void ExprVisitor::visit(const CompareExpr &expr) {
  expr.lhs->visit(*this);
  expr.rhs->visit(*this);
}

void ExprVisitor::visit(const LiteralExpr &expr) {}

void ExprVisitor::visit(const BooleanConst &expr) {}

} // namespace pefa::internal

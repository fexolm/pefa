#pragma once
#include <memory>
#include <string>
#include <utility>
#include <variant>

namespace pefa::internal {

struct ExprVisitor;

struct Expr : std::enable_shared_from_this<Expr> {
  virtual void visit(ExprVisitor &visitor) const = 0;
};

struct BooleanExpr : Expr {
  std::shared_ptr<BooleanExpr> AND(std::shared_ptr<const BooleanExpr> rhs) const;
  std::shared_ptr<BooleanExpr> OR(std::shared_ptr<const BooleanExpr> rhs) const;
};

struct BooleanConst : BooleanExpr {
  const bool value;
  explicit BooleanConst(bool val);
  static std::shared_ptr<BooleanConst> create(bool value);
  void visit(ExprVisitor &visitor) const override;
};

struct PredicateExpr : BooleanExpr {
  enum class Op {
    AND,
    OR,
  };

  std::shared_ptr<const BooleanExpr> lhs;
  std::shared_ptr<const BooleanExpr> rhs;
  const Op op;

  PredicateExpr(std::shared_ptr<const BooleanExpr> lhs, std::shared_ptr<const BooleanExpr> rhs,
                Op op);
  static std::shared_ptr<PredicateExpr> create(std::shared_ptr<const BooleanExpr> lhs,
                                               std::shared_ptr<const BooleanExpr> rhs, Op op);
  void visit(ExprVisitor &visitor) const override;
};

struct LiteralExpr;
struct ColumnRef;

struct CompareExpr : BooleanExpr {
  enum class Op {
    GT,
    LT,
    GE,
    LE,
    EQ,
    NEQ,
  };
  std::shared_ptr<const ColumnRef> lhs;
  std::shared_ptr<const LiteralExpr> rhs;
  const Op op;

  CompareExpr(std::shared_ptr<const ColumnRef> lhs, std::shared_ptr<const LiteralExpr> rhs, Op op);

  static std::shared_ptr<CompareExpr> create(std::shared_ptr<const ColumnRef> lhs,
                                             std::shared_ptr<const LiteralExpr> rhs, Op op);
  void visit(ExprVisitor &visitor) const override;
};

struct ColumnRef : Expr {
  const std::string name;

  explicit ColumnRef(std::string name);
  static std::shared_ptr<ColumnRef> create(std::string name);
  void visit(ExprVisitor &visitor) const override;

  std::shared_ptr<CompareExpr> EQ(std::shared_ptr<const LiteralExpr> rhs) const;
  std::shared_ptr<CompareExpr> LE(std::shared_ptr<const LiteralExpr> rhs) const;
  std::shared_ptr<CompareExpr> GE(std::shared_ptr<const LiteralExpr> rhs) const;
  std::shared_ptr<CompareExpr> NEQ(std::shared_ptr<const LiteralExpr> rhs) const;
  std::shared_ptr<CompareExpr> LT(std::shared_ptr<const LiteralExpr> rhs) const;
  std::shared_ptr<CompareExpr> GT(std::shared_ptr<const LiteralExpr> rhs) const;
};

struct LiteralExpr : Expr {
  const std::variant<int, double, std::string, bool> value;

  explicit LiteralExpr(std::variant<int, double, std::string, bool> val);
  static std::shared_ptr<LiteralExpr>
  create(const std::variant<int, double, std::string, bool> &val);
  void visit(ExprVisitor &visitor) const override;
};

struct ExprVisitor {
  virtual void visit(const ColumnRef &expr);
  virtual void visit(const PredicateExpr &expr);
  virtual void visit(const CompareExpr &expr);
  virtual void visit(const LiteralExpr &expr);
  virtual void visit(const BooleanConst &expr);
};
} // namespace pefa::internal

namespace pefa {
inline std::shared_ptr<internal::ColumnRef> col(std::string name) {
  return internal::ColumnRef::create(std::move(name));
}

inline std::shared_ptr<internal::LiteralExpr>
lit(const std::variant<int, double, std::string, bool> &val) {
  return internal::LiteralExpr::create(val);
}
} // namespace pefa

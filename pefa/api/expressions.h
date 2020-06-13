#pragma once
#include <memory>
#include <variant>
#include <string>

namespace pefa::internal {

struct ExprVisitor;

struct Expr : std::enable_shared_from_this<Expr> {
  virtual void visit(ExprVisitor &visitor) const = 0;
};

struct BooleanExpr : Expr {
  std::shared_ptr<BooleanExpr> AND(const std::shared_ptr<const BooleanExpr> rhs) const;
  std::shared_ptr<BooleanExpr> OR(const std::shared_ptr<const BooleanExpr> rhs) const;
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

  const std::shared_ptr<const BooleanExpr> lhs;
  const std::shared_ptr<const BooleanExpr> rhs;
  const Op op;

  PredicateExpr(const std::shared_ptr<const BooleanExpr> lhs,
                const std::shared_ptr<const BooleanExpr> rhs, Op op);
  static std::shared_ptr<PredicateExpr> create(const std::shared_ptr<const BooleanExpr> lhs,
                                               const std::shared_ptr<const BooleanExpr> rhs, Op op);
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
  const std::shared_ptr<const ColumnRef> lhs;
  const std::shared_ptr<const LiteralExpr> rhs;
  const Op op;

  CompareExpr(const std::shared_ptr<const ColumnRef> lhs,
              const std::shared_ptr<const LiteralExpr> rhs, Op op);

  static std::shared_ptr<CompareExpr> create(const std::shared_ptr<const ColumnRef> lhs,
                                             const std::shared_ptr<const LiteralExpr> rhs, Op op);
  void visit(ExprVisitor &visitor) const override;
};

struct ColumnRef : Expr {
  const std::string name;

  explicit ColumnRef(std::string name);
  static std::shared_ptr<ColumnRef> create(std::string name);
  void visit(ExprVisitor &visitor) const override;

  std::shared_ptr<CompareExpr> EQ(const std::shared_ptr<const LiteralExpr> rhs) const;
  std::shared_ptr<CompareExpr> LE(const std::shared_ptr<const LiteralExpr> rhs) const;
  std::shared_ptr<CompareExpr> GE(const std::shared_ptr<const LiteralExpr> rhs) const;
  std::shared_ptr<CompareExpr> NEQ(const std::shared_ptr<const LiteralExpr> rhs) const;
  std::shared_ptr<CompareExpr> LT(const std::shared_ptr<const LiteralExpr> rhs) const;
  std::shared_ptr<CompareExpr> GT(const std::shared_ptr<const LiteralExpr> rhs) const;
};

struct LiteralExpr : Expr {
  const std::variant<int64_t, double, std::string, bool> value;

  explicit LiteralExpr(std::variant<int64_t, double, std::string, bool> val);
  static std::shared_ptr<LiteralExpr> create(std::variant<int64_t, double, std::string, bool> val);
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
  return internal::ColumnRef::create(name);
}

inline std::shared_ptr<internal::LiteralExpr>
lit(std::variant<int64_t, double, std::string, bool> val) {
  return internal::LiteralExpr::create(val);
}
} // namespace pefa

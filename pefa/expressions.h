#pragma once
#include <memory>
#include <variant>

namespace pefa::internal {

struct ExprVisitor;

struct Expr : std::enable_shared_from_this<Expr> {
  virtual void visit(ExprVisitor &visitor) = 0;
};

struct BooleanExpr : Expr {
  std::shared_ptr<BooleanExpr> AND(std::shared_ptr<BooleanExpr> rhs);
  std::shared_ptr<BooleanExpr> OR(std::shared_ptr<BooleanExpr> rhs);
};

struct PredicateExpr : BooleanExpr {
  enum class Op {
    AND,
    OR,
  };

  const std::shared_ptr<BooleanExpr> lhs;
  const std::shared_ptr<BooleanExpr> rhs;
  const Op op;

  PredicateExpr(std::shared_ptr<BooleanExpr> lhs, std::shared_ptr<BooleanExpr> rhs, Op op);
  static std::shared_ptr<PredicateExpr> create(std::shared_ptr<BooleanExpr> lhs,
                                               std::shared_ptr<BooleanExpr> rhs, Op op);
  void visit(ExprVisitor &visitor) override;
};

struct CompareExpr;
struct ComparebleExpr : Expr {
  std::shared_ptr<CompareExpr> EQ(std::shared_ptr<ComparebleExpr> rhs);
  std::shared_ptr<CompareExpr> LE(std::shared_ptr<ComparebleExpr> rhs);
  std::shared_ptr<CompareExpr> GE(std::shared_ptr<ComparebleExpr> rhs);
  std::shared_ptr<CompareExpr> NEQ(std::shared_ptr<ComparebleExpr> rhs);
  std::shared_ptr<CompareExpr> LT(std::shared_ptr<ComparebleExpr> rhs);
  std::shared_ptr<CompareExpr> GT(std::shared_ptr<ComparebleExpr> rhs);
};

struct CompareExpr : BooleanExpr {
  enum class Op {
    GT,
    LT,
    GE,
    LE,
    EQ,
    NEQ,
  };
  const std::shared_ptr<ComparebleExpr> lhs;
  const std::shared_ptr<ComparebleExpr> rhs;
  const Op op;

  CompareExpr(std::shared_ptr<ComparebleExpr> lhs, std::shared_ptr<ComparebleExpr> rhs, Op op);

  static std::shared_ptr<CompareExpr> create(std::shared_ptr<ComparebleExpr> lhs,
                                             std::shared_ptr<ComparebleExpr> rhs, Op op);
  void visit(ExprVisitor &visitor) override;
};

struct ColumnRef : ComparebleExpr {
  const std::string name;

  explicit ColumnRef(std::string name);
  static std::shared_ptr<ColumnRef> create(std::string name);
  void visit(ExprVisitor &visitor) override;
};

struct LiteralExpr : ComparebleExpr {
  const std::variant<int64_t, double, std::string> value;

  explicit LiteralExpr(std::variant<int64_t, double, std::string> val);
  static std::shared_ptr<LiteralExpr> create(std::variant<int64_t, double, std::string> val);
  void visit(ExprVisitor &visitor) override;
};

struct ExprVisitor {
  virtual void visit(const ColumnRef &expr);
  virtual void visit(const PredicateExpr &expr);
  virtual void visit(const CompareExpr &expr);
  virtual void visit(const LiteralExpr &expr);
};
} // namespace pefa::internal

namespace pefa {
inline std::shared_ptr<internal::ColumnRef> col(std::string name) {
  return internal::ColumnRef::create(name);
}

inline std::shared_ptr<internal::LiteralExpr> lit(std::variant<int64_t, double, std::string> val) {
  return internal::LiteralExpr::create(val);
}
} // namespace pefa

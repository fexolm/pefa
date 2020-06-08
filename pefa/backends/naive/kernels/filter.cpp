#include "filter.h"

#include "pefa/expressions/expressions.h"
#include "pefa/jit/jit.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>

namespace pefa::backends::naive::kernels {
using namespace internal;
std::unique_ptr<llvm::Module> gen_filter(llvm::LLVMContext &context,
                                         std::shared_ptr<arrow::Field> field,
                                         std::shared_ptr<Expr> expr);

void gen_predicate_func(llvm::LLVMContext &context, llvm::Module &module,
                        std::shared_ptr<arrow::Field> field, std::shared_ptr<Expr> expr);

void gen_filter_func(llvm::LLVMContext &context, llvm::Module &module,
                     std::shared_ptr<arrow::Field> field);

class IrEmitVisitor : public ExprVisitor {
private:
  llvm::Value *m_result;
  llvm::IRBuilder<> *m_builder;
  llvm::LLVMContext *m_context;
  std::shared_ptr<arrow::Field> m_field;
  llvm::Value *m_input;

  // we need to keep last op to generate proper constant (true/false)
  // if expr does not operate with referenced column
  PredicateExpr::Op m_last_op{PredicateExpr::Op::AND};

public:
  IrEmitVisitor(llvm::LLVMContext *context, llvm::IRBuilder<> *builder,
                std::shared_ptr<arrow::Field> field, llvm::Value *input)
      : m_result(nullptr), m_builder(builder), m_context(context), m_field(field), m_input(input) {}

  void visit(const PredicateExpr &expr) override {
    m_last_op = expr.op;
    expr.lhs->visit(*this);
    auto lhs = m_result;
    m_last_op = expr.op;
    expr.rhs->visit(*this);
    auto rhs = m_result;
    switch (expr.op) {
    case PredicateExpr::Op::OR:
      m_result = m_builder->CreateOr(lhs, rhs);
      break;
    case PredicateExpr::Op::AND:
      m_result = m_builder->CreateAnd(lhs, rhs);
      break;
    }
  }

  void visit(const CompareExpr &expr) override {
    if (expr.lhs->name == m_field->name()) {
      switch (expr.op) {
      case CompareExpr::Op::GT:
        switch (m_field->type()->id()) {
        case arrow::Type::INT64: {
          auto i64_typ = llvm::Type::getInt64Ty(*m_context);
          m_result = m_builder->CreateICmpSGT(
              m_input, llvm::ConstantInt::get(i64_typ, std::get<int64_t>(expr.rhs->value), true));
          break;
        }
        default:
          throw "not implemented exception"; // TODO: implement not implemented exception
        }
        break;
      case CompareExpr::Op::LT:
        switch (m_field->type()->id()) {
        case arrow::Type::INT64: {
          auto i64_typ = llvm::Type::getInt64Ty(*m_context);
          m_result = m_builder->CreateICmpSLT(
              m_input, llvm::ConstantInt::get(i64_typ, std::get<int64_t>(expr.rhs->value), true));
          break;
        }
        default:
          throw "not implemented exception"; // TODO: implement not implemented exception
        }
        break;
      case CompareExpr::Op::GE:
        switch (m_field->type()->id()) {
        case arrow::Type::INT64: {
          auto i64_typ = llvm::Type::getInt64Ty(*m_context);
          m_result = m_builder->CreateICmpSGE(
              m_input, llvm::ConstantInt::get(i64_typ, std::get<int64_t>(expr.rhs->value), true));
          break;
        }
        default:
          throw "not implemented exception"; // TODO: implement not implemented exception
        }
        break;
      case CompareExpr::Op::LE:
        switch (m_field->type()->id()) {
        case arrow::Type::INT64: {
          auto i64_typ = llvm::Type::getInt64Ty(*m_context);
          m_result = m_builder->CreateICmpSLE(
              m_input, llvm::ConstantInt::get(i64_typ, std::get<int64_t>(expr.rhs->value), true));
          break;
        }
        default:
          throw "not implemented exception"; // TODO: implement not implemented exception
        }
        break;
      case CompareExpr::Op::EQ:
        switch (m_field->type()->id()) {
        case arrow::Type::INT64: {
          auto i64_typ = llvm::Type::getInt64Ty(*m_context);
          m_result = m_builder->CreateICmpEQ(
              m_input, llvm::ConstantInt::get(i64_typ, std::get<int64_t>(expr.rhs->value), true));
          break;
        }
        default:
          throw "not implemented exception"; // TODO: implement not implemented exception
        }
        break;
      }
    } else {
      auto bool_typ = llvm::Type::getInt8Ty(*m_context);
      switch(m_last_op) {
      case PredicateExpr::Op::OR:
        m_result = llvm::ConstantInt::get(bool_typ, 0, true);
        break;
      case PredicateExpr::Op::AND:
        m_result = llvm::ConstantInt::get(bool_typ, 1, true);
        break;
      }

    }
  }

  llvm::Value *result() {
    return m_result;
  }
}; // namespace pefa::backends::naive::kernels

void gen_filters(std::shared_ptr<arrow::Schema> schema, std::shared_ptr<Expr> expr) {
  auto jit = jit::get_JIT();
  static llvm::LLVMContext context;
  for (int i = 0; i < schema->num_fields(); i++) {
    auto module = gen_filter(context, schema->field(i), expr);
    auto &machine = jit->getTargetMachine();
    auto layout = machine.createDataLayout();
    module->setDataLayout(layout);
    jit->addModule(std::move(module));
  }
}

std::unique_ptr<llvm::Module> gen_filter(llvm::LLVMContext &context,
                                         std::shared_ptr<arrow::Field> field,
                                         std::shared_ptr<Expr> expr) {
  auto module = std::make_unique<llvm::Module>(field->name() + "_filter_mod", context);

  gen_predicate_func(context, *module, field, expr);
  gen_filter_func(context, *module, field);

  return module;
}

llvm::Type *to_llvm_type(llvm::LLVMContext &context, const arrow::DataType &type) {
  switch (type.id()) {
  case arrow::Type::INT64:
    return llvm::Type::getInt64Ty(context);
  default:
    throw "not implemented exception"; // TODO: implement not implemented exception
  }
}

void gen_predicate_func(llvm::LLVMContext &context, llvm::Module &module,
                        std::shared_ptr<arrow::Field> field, std::shared_ptr<Expr> expr) {
  std::vector<llvm::Type *> param_type{to_llvm_type(context, *field->type())};
  llvm::FunctionType *prototype =
      llvm::FunctionType::get(llvm::Type::getInt8Ty(context), param_type, false);
  llvm::Function *func = llvm::Function::Create(prototype, llvm::Function::ExternalLinkage,
                                                field->name() + "_predicate", module);
  llvm::BasicBlock *body = llvm::BasicBlock::Create(context, "body", func);
  auto args = func->arg_begin();
  llvm::Value *val = &(*args);
  llvm::IRBuilder builder(context);
  builder.SetInsertPoint(body);

  IrEmitVisitor visitor(&context, &builder, field, val);
  expr->visit(visitor);
  builder.CreateRet(visitor.result());
}

void gen_filter_func(llvm::LLVMContext &context, llvm::Module &module,
                     std::shared_ptr<arrow::Field> field) {
  std::vector<llvm::Type *> param_type{llvm::Type::getInt8PtrTy(context, 1),
                                       llvm::Type::getInt8PtrTy(context, 1),
                                       llvm::Type::getInt64Ty(context)};

  llvm::FunctionType *prototype =
      llvm::FunctionType::get(llvm::Type::getVoidTy(context), param_type, false);
  llvm::Function *func = llvm::Function::Create(prototype, llvm::Function::ExternalLinkage,
                                                field->name() + "_filter", module);
  llvm::BasicBlock *body = llvm::BasicBlock::Create(context, "body", func);

  auto args = func->arg_begin();
  llvm::Value *arg_source = &(*args);
  args = std::next(args);
  llvm::Value *arg_dest = &(*args);
  args = std::next(args);
  llvm::Value *arg_len = &(*args);

  llvm::IRBuilder builder(context);
  builder.SetInsertPoint(body);
  auto *dest = builder.CreateAddrSpaceCast(arg_source,
                                           to_llvm_type(context, *field->type())->getPointerTo());
  auto *val = builder.CreateLoad(arg_source);
  builder.CreateStore(val, arg_dest);
  builder.CreateRetVoid();
}

// void filter(int64_t *a, int8_t *d, int len) {
//  for (int i = 0; i < len / 8; i++) {
//    auto src = a + i * 8;
//    for (int j = 0; j < 8; j++) {
//      dst[i] |=
//    }
//  }
//}
} // namespace pefa::backends::naive::kernels
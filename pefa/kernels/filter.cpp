#include "filter.h"

#include "pefa/jit/jit.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

namespace pefa::internal::kernels {

llvm::Type *to_llvm_type(llvm::LLVMContext &context, const arrow::DataType &type) {
  switch (type.id()) {
  case arrow::Type::INT64:
    return llvm::Type::getInt64Ty(context);
  default:
    throw "not implemented exception"; // TODO: implement not implemented exception
  }
}

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
  IrEmitVisitor(llvm::LLVMContext *context, llvm::IRBuilder<> *builder, std::shared_ptr<arrow::Field> field, llvm::Value *input)
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
          m_result = m_builder->CreateICmpSGT(m_input, llvm::ConstantInt::get(i64_typ, std::get<int64_t>(expr.rhs->value), true));
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
          m_result = m_builder->CreateICmpSLT(m_input, llvm::ConstantInt::get(i64_typ, std::get<int64_t>(expr.rhs->value), true));
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
          m_result = m_builder->CreateICmpSGE(m_input, llvm::ConstantInt::get(i64_typ, std::get<int64_t>(expr.rhs->value), true));
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
          m_result = m_builder->CreateICmpSLE(m_input, llvm::ConstantInt::get(i64_typ, std::get<int64_t>(expr.rhs->value), true));
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
          m_result = m_builder->CreateICmpEQ(m_input, llvm::ConstantInt::get(i64_typ, std::get<int64_t>(expr.rhs->value), true));
          break;
        }
        default:
          throw "not implemented exception"; // TODO: implement not implemented exception
        }
        break;
      }
    } else {
      auto bool_typ = llvm::Type::getInt1Ty(*m_context);
      switch (m_last_op) {
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

class FitlerKernelImpl : public FilterKernel {
private:
  std::shared_ptr<arrow::Field> m_field;
  std::shared_ptr<Expr> m_expr;
  llvm::LLVMContext m_context;
  llvm::orc::VModuleKey m_moduleKey;
  bool m_is_compiled = false;
  std::shared_ptr<pefa::jit::JIT> m_jit;
  void (*m_func)(const uint8_t *, uint8_t *, int64_t);

public:
  FitlerKernelImpl(std::shared_ptr<arrow::Field> field, std::shared_ptr<Expr> expr)
      : m_field(field), m_expr(expr), m_context(llvm::LLVMContext()), m_jit(jit::get_JIT()) {}

  void execute(std::shared_ptr<arrow::Array> column, std::shared_ptr<arrow::UInt8Array> bitmap) override {
    compile();
    m_func(column->data()->buffers[1]->data(), bitmap->data()->buffers[1]->mutable_data(), column->length());
  }

  ~FitlerKernelImpl() {
    if (m_is_compiled) {
      m_jit->removeModule(m_moduleKey);
    }
  }

private:
  void compile() {
    if (!m_is_compiled) {
      auto module = std::make_unique<llvm::Module>(m_field->name() + "_filter_mod", m_context);
      gen_predicate_func(*module);
      gen_filter_func(*module);
      auto &machine = m_jit->getTargetMachine();
      auto layout = machine.createDataLayout();
      module->setDataLayout(layout);
      m_moduleKey = m_jit->addModule(std::move(module));
      auto sym = m_jit->findSymbol(m_field->name() + "_filter");
      m_func = reinterpret_cast<void (*)(const uint8_t *, uint8_t *, int64_t)>(sym.getAddress().get());
      m_is_compiled = true;
    }
  }

  void gen_predicate_func(llvm::Module &module) {
    std::vector<llvm::Type *> param_type{to_llvm_type(m_context, *m_field->type())};
    llvm::FunctionType *prototype = llvm::FunctionType::get(llvm::Type::getInt1Ty(m_context), param_type, false);
    llvm::Function *func = llvm::Function::Create(prototype, llvm::Function::InternalLinkage, m_field->name() + "_predicate", module);
    llvm::BasicBlock *body = llvm::BasicBlock::Create(m_context, "body", func);
    auto args = func->arg_begin();
    llvm::Value *val = &(*args);
    llvm::IRBuilder builder(m_context);
    builder.SetInsertPoint(body);

    IrEmitVisitor visitor(&m_context, &builder, m_field, val);
    m_expr->visit(visitor);
    builder.CreateRet(visitor.result());
  }

  void gen_filter_func(llvm::Module &module) {
    std::vector<llvm::Type *> param_type{llvm::Type::getInt8PtrTy(m_context, 1), llvm::Type::getInt8PtrTy(m_context, 1),
                                         llvm::Type::getInt64Ty(m_context)};

    llvm::FunctionType *prototype = llvm::FunctionType::get(llvm::Type::getVoidTy(m_context), param_type, false);
    llvm::Function *func = llvm::Function::Create(prototype, llvm::Function::ExternalLinkage, m_field->name() + "_filter", module);
    llvm::BasicBlock *body = llvm::BasicBlock::Create(m_context, "body", func);
    llvm::BasicBlock *cond_1 = llvm::BasicBlock::Create(m_context, "cond_1", func);
    llvm::BasicBlock *for_1 = llvm::BasicBlock::Create(m_context, "for_1", func);
    llvm::BasicBlock *cond_2 = llvm::BasicBlock::Create(m_context, "cond_2", func);
    llvm::BasicBlock *for_2 = llvm::BasicBlock::Create(m_context, "for_2", func);
    llvm::BasicBlock *end_for1 = llvm::BasicBlock::Create(m_context, "end_for1", func);
    llvm::BasicBlock *end_for2 = llvm::BasicBlock::Create(m_context, "end_for2", func);

    auto args = func->arg_begin();
    llvm::Value *arg_source = &(*args);
    args = std::next(args);
    llvm::Value *arg_dest = &(*args);
    args = std::next(args);
    llvm::Value *arg_len = &(*args);

    llvm::IRBuilder builder(m_context);
    builder.SetInsertPoint(body);
    auto i64_t = llvm::Type::getInt64Ty(m_context);
    auto i8_t = llvm::Type::getInt8Ty(m_context);
    auto *i = builder.CreateAlloca(i64_t);
    auto *j = builder.CreateAlloca(i64_t);
    builder.CreateStore(llvm::ConstantInt::get(i64_t, 0), i);
    builder.CreateStore(llvm::ConstantInt::get(i64_t, 0), j);

    auto *source = builder.CreateAddrSpaceCast(arg_source, to_llvm_type(m_context, *m_field->type())->getPointerTo());
    builder.CreateBr(cond_1);
    builder.SetInsertPoint(cond_1);
    builder.CreateCondBr(builder.CreateICmpSLT(builder.CreateLoad(i), builder.CreateUDiv(arg_len, llvm::ConstantInt::get(i64_t, 8))), for_1,
                         end_for1);
    builder.SetInsertPoint(for_1);
    auto src = builder.CreateGEP(source, builder.CreateMul(builder.CreateLoad(i), llvm::ConstantInt::get(i64_t, 8)), "src");
    builder.CreateStore(llvm::ConstantInt::get(i64_t, 0), j);
    builder.CreateBr(cond_2);
    builder.SetInsertPoint(cond_2);
    builder.CreateCondBr(builder.CreateICmpSLT(builder.CreateLoad(j), llvm::ConstantInt::get(i64_t, 8)), for_2, end_for2);
    builder.SetInsertPoint(for_2);
    auto bit = builder.CreateIntCast(builder.CreateCall(module.getFunction(m_field->name() + "_predicate"),
                                                        {builder.CreateLoad(builder.CreateGEP(src, builder.CreateLoad(j)))}),
                                     i8_t, false);
    auto bit_with_shift = builder.CreateShl(
        bit, builder.CreateSub(llvm::ConstantInt::get(i8_t, 7), builder.CreateIntCast(builder.CreateLoad(j), i8_t, false)));
    auto updated_bitmap = builder.CreateOr(builder.CreateLoad(builder.CreateGEP(arg_dest, builder.CreateLoad(i))), bit_with_shift);
    builder.CreateStore(updated_bitmap, builder.CreateGEP(arg_dest, builder.CreateLoad(i)));
    builder.CreateStore(builder.CreateAdd(builder.CreateLoad(j), llvm::ConstantInt::get(i64_t, 1)), j);
    builder.CreateBr(cond_2);
    builder.SetInsertPoint(end_for2);
    builder.CreateStore(builder.CreateAdd(builder.CreateLoad(i), llvm::ConstantInt::get(i64_t, 1)), i);
    builder.CreateBr(cond_1);
    builder.SetInsertPoint(end_for1);
    builder.CreateRetVoid();
  }
};

std::unique_ptr<FilterKernel> FilterKernel::create_cpu(std::shared_ptr<arrow::Field> field, std::shared_ptr<Expr> expr) {
  return std::make_unique<FitlerKernelImpl>(field, expr);
}
} // namespace pefa::internal::kernels
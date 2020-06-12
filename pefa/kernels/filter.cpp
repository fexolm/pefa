#include "filter.h"

#include "pefa/api/exceptions.h"
#include "pefa/jit/jit.h"
#include "pefa/utils/llvm_helpers.h"
#include "pefa/utils/utils.h"

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Host.h>

namespace pefa::internal::kernels {

class IrEmitVisitor : public ExprVisitor, private utils::LLVMTypesHelper {
private:
  llvm::Value *m_result;
  llvm::IRBuilder<> *m_builder;
  llvm::LLVMContext *m_context;
  const std::shared_ptr<const arrow::Field> m_field;
  llvm::Value *m_input;

  // we need to keep last op to generate proper constant (true/false)
  // if expr does not operate with referenced column
  PredicateExpr::Op m_last_op{PredicateExpr::Op::AND};

public:
  IrEmitVisitor(llvm::LLVMContext *context, llvm::IRBuilder<> *builder,
                const std::shared_ptr<const arrow::Field> field, llvm::Value *input)
      : utils::LLVMTypesHelper(*context)
      , m_result(nullptr)
      , m_builder(builder)
      , m_context(context)
      , m_field(field)
      , m_input(input) {}

  void visit(const PredicateExpr &expr) override {
    m_last_op = expr.op;
    expr.lhs->visit(*this);
    auto lhs = m_result;
    m_last_op = expr.op;
    expr.rhs->visit(*this);
    auto rhs = m_result;
    switch (expr.op) {
      PEFA_CASE_BRK(case PredicateExpr::Op::OR:, m_result = m_builder->CreateOr(lhs, rhs))
      PEFA_CASE_BRK(case PredicateExpr::Op::AND:, m_result = m_builder->CreateAnd(lhs, rhs))
    }
  }

  void visit(const CompareExpr &expr) override {
    if (expr.lhs->name == m_field->name()) {
      auto &typ = *(m_field->type());
      auto constant = const_from_variant(typ, expr.rhs->value);
      switch (expr.op) {
        PEFA_CASE_BRK(case CompareExpr::Op::GT:,
                      m_result = create_cmp_gt(typ, *m_builder, m_input, constant))
        PEFA_CASE_BRK(case CompareExpr::Op::LT:,
                      m_result = create_cmp_lt(typ, *m_builder, m_input, constant))
        PEFA_CASE_BRK(case CompareExpr::Op::GE:,
                      m_result = create_cmp_ge(typ, *m_builder, m_input, constant))
        PEFA_CASE_BRK(case CompareExpr::Op::LE:,
                      m_result = create_cmp_le(typ, *m_builder, m_input, constant))
        PEFA_CASE_BRK(case CompareExpr::Op::EQ:,
                      m_result = create_cmp_eq(typ, *m_builder, m_input, constant))
        PEFA_CASE_BRK(case CompareExpr::Op::NEQ:,
                      m_result = create_cmp_ne(typ, *m_builder, m_input, constant))
      }
    } else {
      switch (m_last_op) {
        PEFA_CASE_BRK(case PredicateExpr::Op::OR:, m_result = boolval(false))
        PEFA_CASE_BRK(case PredicateExpr::Op::AND:, m_result = boolval(true))
      }
    }
  }

  llvm::Value *result() {
    return m_result;
  }
}; // namespace pefa::backends::naive::kernels

class FitlerKernelImpl : public FilterKernel, private utils::LLVMTypesHelper {
private:
  const std::shared_ptr<const arrow::Field> m_field;
  const std::shared_ptr<const Expr> m_expr;
  llvm::LLVMContext m_context;
  llvm::orc::VModuleKey m_moduleKey;
  bool m_is_compiled = false;
  std::shared_ptr<pefa::jit::JIT> m_jit;
  void (*m_func)(const uint8_t *, uint8_t *, int64_t);

public:
  FitlerKernelImpl(const std::shared_ptr<const arrow::Field> field, const std::shared_ptr<const Expr> expr)
      : m_field(field)
      , m_expr(expr)
      , m_context(llvm::LLVMContext())
      , m_jit(jit::get_JIT())
      , utils::LLVMTypesHelper(m_context) {}

  void execute(const std::shared_ptr<arrow::Array> column, uint8_t *bitmap, size_t offset) override {
    if (!m_is_compiled) {
      throw KernelNotCompiledException();
    }
    if (auto type = dynamic_cast<arrow::FixedWidthType *>(column->type().get())) {
      m_func(column->data()->buffers[1]->data() + (type->bit_width() * offset / 8),
             bitmap + (offset != 0), column->length() - offset);
    } else {
      throw NotImplementedException("Variable length type filtering does not implemented yet");
    }
  }

  ~FitlerKernelImpl() {
    if (m_is_compiled) {
      m_jit->removeModule(m_moduleKey);
    }
  }

  void compile() override {
    auto module = std::make_unique<llvm::Module>(m_field->name() + "_filter_mod", m_context);
    module->setTargetTriple(llvm::sys::getProcessTriple());
    gen_predicate_func(*module);
    gen_filter_func(*module);
    auto &machine = m_jit->getTargetMachine();
    auto layout = machine.createDataLayout();
    module->setDataLayout(layout);
    m_moduleKey = m_jit->addModule(std::move(module));
    auto sym = m_jit->findSymbol(m_field->name() + "_filter");
    m_func =
        reinterpret_cast<void (*)(const uint8_t *, uint8_t *, int64_t)>(sym.getAddress().get());
    m_is_compiled = true;
  }

private:
  void gen_predicate_func(llvm::Module &module) {
    std::vector<llvm::Type *> param_type{from_arrow(*m_field->type())};
    llvm::FunctionType *prototype =
        llvm::FunctionType::get(llvm::Type::getInt1Ty(m_context), param_type, false);
    llvm::Function *func = llvm::Function::Create(prototype, llvm::Function::InternalLinkage,
                                                  m_field->name() + "_predicate", module);
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
    // void filter(int *in, char *out, int len) {
    //     for(int i=0; i<len / 8; i++) {
    //         int *src = in + i * 8;
    //         char bitmap = 0;
    //         for(int j=0; j<8; j++) {
    //            bitmap |= predicate(src[j]) << (7 - j);
    //         }
    //         out[i] &= bitmap;
    //     }
    // }
    std::vector<llvm::Type *> param_type{llvm::Type::getInt8PtrTy(m_context),
                                         llvm::Type::getInt8PtrTy(m_context),
                                         llvm::Type::getInt64Ty(m_context)};

    llvm::FunctionType *prototype =
        llvm::FunctionType::get(llvm::Type::getVoidTy(m_context), param_type, false);
    llvm::Function *func = llvm::Function::Create(prototype, llvm::Function::ExternalLinkage,
                                                  m_field->name() + "_filter", module);
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
    auto *i = builder.CreateAlloca(i64_typ(), nullptr, "i");
    auto *j = builder.CreateAlloca(i8_typ(), nullptr, "j");
    auto *tmp_bitmap = builder.CreateAlloca(i8_typ(), nullptr, "tmp_bitmap");
    builder.CreateStore(i64val(0), i);
    builder.CreateStore(i8val(0), j);
    auto *source = builder.CreatePointerCast(arg_source, ptr_from_arrow(*m_field->type()));
    builder.CreateBr(cond_1);

    builder.SetInsertPoint(cond_1);
    auto condition =
        builder.CreateICmpSLT(builder.CreateLoad(i), builder.CreateUDiv(arg_len, i64val(8)));
    builder.CreateCondBr(condition, for_1, end_for1);

    builder.SetInsertPoint(for_1);
    auto li = builder.CreateLoad(i);
    auto src = builder.CreateInBoundsGEP(source, builder.CreateMul(li, i64val(8)), "src");
    builder.CreateStore(i8val(0), j);
    builder.CreateStore(i8val(0), tmp_bitmap);
    builder.CreateBr(cond_2);

    builder.SetInsertPoint(cond_2);
    builder.CreateCondBr(builder.CreateICmpSLT(builder.CreateLoad(j), i8val(8)), for_2, end_for2);

    builder.SetInsertPoint(for_2);
    auto bit = builder.CreateIntCast(
        builder.CreateCall(
            module.getFunction(m_field->name() + "_predicate"),
            {builder.CreateLoad(builder.CreateInBoundsGEP(src, builder.CreateLoad(j)))}),
        i8_typ(), false);

    auto bit_with_shift =
        builder.CreateShl(bit, builder.CreateSub(i8val(7), builder.CreateLoad(j)));

    auto updated_tmp_bitmap = builder.CreateOr(builder.CreateLoad(tmp_bitmap), bit_with_shift);
    builder.CreateStore(updated_tmp_bitmap, tmp_bitmap);

    builder.CreateStore(builder.CreateAdd(builder.CreateLoad(j), i8val(1)), j);
    builder.CreateBr(cond_2);

    builder.SetInsertPoint(end_for2);

    auto dest_val = builder.CreateLoad(builder.CreateInBoundsGEP(arg_dest, builder.CreateLoad(i)));
    auto updated_bitmap = builder.CreateAnd(dest_val, builder.CreateLoad(tmp_bitmap));

    builder.CreateStore(updated_bitmap, builder.CreateInBoundsGEP(arg_dest, builder.CreateLoad(i)));
    builder.CreateStore(builder.CreateAdd(builder.CreateLoad(i), i64val(1)), i);
    builder.CreateBr(cond_1);

    builder.SetInsertPoint(end_for1);
    builder.CreateRetVoid();
  }
};

std::unique_ptr<FilterKernel> FilterKernel::create_cpu(const std::shared_ptr<arrow::Field> field,
                                                       const std::shared_ptr<Expr> expr) {
  return std::make_unique<FitlerKernelImpl>(field, expr);
}
} // namespace pefa::internal::kernels
#include "filter.h"

#include "pefa/jit/jit.h"
#include "pefa/query_compiler/expressions.h"
#include "pefa/utils/exceptions.h"
#include "pefa/utils/llvm_helpers.h"
#include "pefa/utils/utils.h"

#include <algorithm>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Host.h>
#include <utility>

namespace pefa::kernels {
class IrEmitVisitor : public ExprVisitor, private utils::LLVMTypesHelper {
private:
  llvm::Value *m_result;
  llvm::IRBuilder<> *m_builder;
  llvm::LLVMContext *m_context;
  std::shared_ptr<const arrow::Field> m_field;
  llvm::Value *m_input;

  // we need to keep last op to generate proper constant (true/false)
  // if expr does not operate with referenced column
  PredicateExpr::Op m_last_op{PredicateExpr::Op::AND};

public:
  IrEmitVisitor(llvm::LLVMContext *context, llvm::IRBuilder<> *builder,
                std::shared_ptr<const arrow::Field> field, llvm::Value *input)
      : utils::LLVMTypesHelper(*context)
      , m_result(nullptr)
      , m_builder(builder)
      , m_context(context)
      , m_field(std::move(field))
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
      // TODO: implement more informative assert
      assert(false);
    }
  }

  llvm::Value *result() {
    return m_result;
  }
};

class FitlerKernelImpl : public FilterKernel, private utils::LLVMTypesHelper {
private:
  std::shared_ptr<const arrow::Field> m_field;
  std::shared_ptr<const Expr> m_expr;
  llvm::LLVMContext m_context;
  llvm::orc::VModuleKey m_moduleKey{};
  bool m_is_compiled = false;
  std::shared_ptr<pefa::jit::JIT> m_jit;
  void (*m_filter_func)(const uint8_t *, uint8_t *, int64_t){};
  void (*m_filter_remaining_func)(const uint8_t *, uint8_t *, uint8_t, uint8_t){};

public:
  FitlerKernelImpl(std::shared_ptr<const arrow::Field> field, std::shared_ptr<const Expr> expr)
      : m_field(std::move(field))
      , m_expr(std::move(expr))
      , m_context(llvm::LLVMContext())
      , m_jit(jit::get_JIT())
      , utils::LLVMTypesHelper(m_context) {}

  void execute(std::shared_ptr<const arrow::Array> column, uint8_t *bitmap,
               size_t offset) override {
    if (!m_is_compiled) {
      throw KernelNotCompiledException();
    }
    if (auto type = dynamic_cast<arrow::FixedWidthType *>(column->type().get())) {
      m_filter_func(column->data()->buffers[1]->data() + (type->bit_width() * offset / 8),
                    bitmap + (offset != 0), column->length() - offset);
    } else {
      throw NotImplementedException("Variable length type filtering does not implemented yet");
    }
  }

  void execute_remaining(std::shared_ptr<const arrow::Array> column, uint8_t *bitmap,
                         size_t array_offset, uint8_t bit_offset) override {
    if (!m_is_compiled) {
      throw KernelNotCompiledException();
    }
    if (auto type = dynamic_cast<arrow::FixedWidthType *>(column->type().get())) {
      uint8_t len = 0;
      // begining of chunk
      if (array_offset == 0) {
        len = std::min(static_cast<uint8_t>(8 - bit_offset),
                       static_cast<uint8_t>(column->length() - array_offset));
      } else {
        // end of chunk
        len = static_cast<uint8_t>(column->length() - array_offset);
      }
      m_filter_remaining_func(column->data()->buffers[1]->data() +
                                  (type->bit_width() * array_offset / 8),
                              bitmap, len, bit_offset);
    } else {
      throw NotImplementedException("Variable length type filtering does not implemented yet");
    }
  }

  ~FitlerKernelImpl() override {
    if (m_is_compiled) {
      m_jit->removeModule(m_moduleKey);
    }
  }

  void compile() override {
    // TODO: use VMModuleKey to resolve collision with another module when finding function
    auto module = std::make_unique<llvm::Module>(m_field->name() + "_filter_mod", m_context);
    module->setTargetTriple(llvm::sys::getProcessTriple());
    gen_predicate_func(*module);
    gen_filter_func(*module);
    gen_filter_remaining_func(*module);
    auto &machine = m_jit->getTargetMachine();
    auto layout = machine.createDataLayout();
    module->setDataLayout(layout);
    m_moduleKey = m_jit->addModule(std::move(module));
    m_filter_func = reinterpret_cast<void (*)(const uint8_t *, uint8_t *, int64_t)>(
        m_jit->findSymbol(m_field->name() + "_filter").getAddress().get());
    m_filter_remaining_func =
        reinterpret_cast<void (*)(const uint8_t *, uint8_t *, uint8_t, uint8_t)>(
            m_jit->findSymbol(m_field->name() + "_filter_remaining").getAddress().get());
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
    llvm::Value *val = func->getArg(0);
    llvm::IRBuilder builder(m_context);
    builder.SetInsertPoint(body);

    IrEmitVisitor visitor(&m_context, &builder, m_field, val);
    m_expr->visit(visitor);
    builder.CreateRet(visitor.result());
  }

  void gen_filter_func(llvm::Module &module) {
    // void filter(uint8_t *in, uint8_t *out, uint64_t len) {
    //     TYPE *source = (TYPE *)in;
    //     for(int i=0; i<len / 8; i++) {
    //         int *src = source + i * 8;
    //         uint8_t tmp_bitmap = 0;
    //         for(int j=0; j<8; j++) {
    //            tmp_bitmap |= predicate(src[j]) << (7 - j);
    //         }
    //         out[i] &= tmp_bitmap;
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
    llvm::BasicBlock *cond_1 = llvm::BasicBlock::Create(m_context, "outerloop.cond", func);
    llvm::BasicBlock *for_1 = llvm::BasicBlock::Create(m_context, "outerloop.body", func);
    llvm::BasicBlock *cond_2 = llvm::BasicBlock::Create(m_context, "innerloop.cond", func);
    llvm::BasicBlock *for_2 = llvm::BasicBlock::Create(m_context, "innedloop.body", func);
    llvm::BasicBlock *end_for1 = llvm::BasicBlock::Create(m_context, "outerloop.end", func);
    llvm::BasicBlock *end_for2 = llvm::BasicBlock::Create(m_context, "innerloop.end", func);

    llvm::Value *arg_source = func->getArg(0);
    llvm::Value *arg_dest = func->getArg(1);
    llvm::Value *arg_len = func->getArg(2);

    llvm::IRBuilder builder(m_context);
    builder.SetInsertPoint(body);
    auto *i = builder.CreateAlloca(i64_typ(), nullptr, "i");
    auto *j = builder.CreateAlloca(i8_typ(), nullptr, "j");
    auto *tmp_bitmap = builder.CreateAlloca(i8_typ(), nullptr, "tmp_bitmap");
    builder.CreateStore(i64val(0), i);

    // as filter function takes uint8_t array as input, we need to cast it to field type
    auto *source = builder.CreatePointerCast(arg_source, ptr_from_arrow(*m_field->type()));
    builder.CreateBr(cond_1);

    builder.SetInsertPoint(cond_1);
    // for(int i=0; i<arg_len / 8; i++)
    auto condition =
        builder.CreateICmpSLT(builder.CreateLoad(i), builder.CreateUDiv(arg_len, i64val(8)));
    builder.CreateCondBr(condition, for_1, end_for1);

    builder.SetInsertPoint(for_1);
    auto li = builder.CreateLoad(i);
    // src = source + i * 8
    auto src = builder.CreateInBoundsGEP(source, builder.CreateMul(li, i64val(8)), "src");
    builder.CreateStore(i8val(0), tmp_bitmap);

    builder.CreateStore(i8val(0), j);
    builder.CreateBr(cond_2);

    // for(int j=0; j<8; j++)
    builder.SetInsertPoint(cond_2);
    builder.CreateCondBr(builder.CreateICmpSLT(builder.CreateLoad(j), i8val(8)), for_2, end_for2);

    builder.SetInsertPoint(for_2);

    // bit = predicate(src[j]);
    auto bit = builder.CreateIntCast(
        builder.CreateCall(
            module.getFunction(m_field->name() + "_predicate"),
            {builder.CreateLoad(builder.CreateInBoundsGEP(src, builder.CreateLoad(j)))}),
        i8_typ(), false);

    // bit_with_shift = bit << (7 - j)
    auto bit_with_shift =
        builder.CreateShl(bit, builder.CreateSub(i8val(7), builder.CreateLoad(j)));

    // tmp_bitmap |= bit_with_shift
    auto updated_tmp_bitmap = builder.CreateOr(builder.CreateLoad(tmp_bitmap), bit_with_shift);
    builder.CreateStore(updated_tmp_bitmap, tmp_bitmap);

    // j++
    builder.CreateStore(builder.CreateAdd(builder.CreateLoad(j), i8val(1)), j);
    builder.CreateBr(cond_2);

    // end inner loop
    builder.SetInsertPoint(end_for2);

    // dst[i] &= tmp_bitmap
    auto dest_val = builder.CreateLoad(builder.CreateInBoundsGEP(arg_dest, builder.CreateLoad(i)));
    auto updated_bitmap = builder.CreateAnd(dest_val, builder.CreateLoad(tmp_bitmap));
    builder.CreateStore(updated_bitmap, builder.CreateInBoundsGEP(arg_dest, builder.CreateLoad(i)));

    // i++
    builder.CreateStore(builder.CreateAdd(builder.CreateLoad(i), i64val(1)), i);
    builder.CreateBr(cond_1);

    // end outer loop
    builder.SetInsertPoint(end_for1);
    builder.CreateRetVoid();
  }

  // filters remaining first/last elements
  void gen_filter_remaining_func(llvm::Module &module) {
    std::vector<llvm::Type *> param_type{
        llvm::Type::getInt8PtrTy(m_context), llvm::Type::getInt8PtrTy(m_context),
        llvm::Type::getInt8Ty(m_context), llvm::Type::getInt8Ty(m_context)};

    llvm::FunctionType *prototype =
        llvm::FunctionType::get(llvm::Type::getVoidTy(m_context), param_type, false);

    llvm::Function *func = llvm::Function::Create(prototype, llvm::Function::ExternalLinkage,
                                                  m_field->name() + "_filter_remaining", module);

    llvm::BasicBlock *body = llvm::BasicBlock::Create(m_context, "body", func);
    llvm::BasicBlock *cond = llvm::BasicBlock::Create(m_context, "loop.cond", func);
    llvm::BasicBlock *loop = llvm::BasicBlock::Create(m_context, "loop.body", func);
    llvm::BasicBlock *end_loop = llvm::BasicBlock::Create(m_context, "loop.end", func);

    llvm::Value *arg_source = func->getArg(0);
    llvm::Value *arg_dest = func->getArg(1);
    llvm::Value *arg_len = func->getArg(2);
    llvm::Value *arg_bit_offset = func->getArg(3);

    llvm::IRBuilder builder(m_context);
    builder.SetInsertPoint(body);
    auto *i = builder.CreateAlloca(i8_typ(), nullptr, "i");
    builder.CreateStore(i8val(0), i);
    auto *source = builder.CreatePointerCast(arg_source, ptr_from_arrow(*m_field->type()));
    builder.CreateBr(cond);

    builder.SetInsertPoint(cond);
    auto condition = builder.CreateICmpSLT(builder.CreateLoad(i), arg_len);
    builder.CreateCondBr(condition, loop, end_loop);

    builder.SetInsertPoint(loop);

    // TODO: understand where it is necessary to use signed operations in filter kernel
    auto bit = builder.CreateIntCast(
        builder.CreateCall(
            module.getFunction(m_field->name() + "_predicate"),
            {builder.CreateLoad(builder.CreateInBoundsGEP(source, builder.CreateLoad(i)))}),
        i8_typ(), false);

    // ~(((~bit) & 1) << (7 - i - offset)
    auto masked_bit = builder.CreateNot(builder.CreateShl(
        builder.CreateAnd(builder.CreateNot(bit), i8val(1)),
        builder.CreateSub(i8val(7), builder.CreateAdd(builder.CreateLoad(i), arg_bit_offset))));

    // we assume that arg_dest is already pointed on required byte, so no offset is needed
    builder.CreateStore(builder.CreateAnd(builder.CreateLoad(arg_dest), masked_bit), arg_dest);

    builder.CreateStore(builder.CreateAdd(builder.CreateLoad(i), i8val(1)), i);
    builder.CreateBr(cond);

    builder.SetInsertPoint(end_loop);
    builder.CreateRetVoid();
  }
};

std::unique_ptr<FilterKernel> FilterKernel::create_cpu(std::shared_ptr<const arrow::Field> field,
                                                       std::shared_ptr<const Expr> expr) {
  return std::make_unique<FitlerKernelImpl>(std::move(field), std::move(expr));
}
} // namespace pefa::kernels
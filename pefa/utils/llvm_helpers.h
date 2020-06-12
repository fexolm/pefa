#pragma once
#include "pefa/api/exceptions.h"
#include "utils.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

namespace pefa::internal::utils {
class LLVMTypesHelper {
private:
  LLVMContext &m_context;

public:
  LLVMTypesHelper(LLVMContext &context)
      : m_context(context) {}

  llvm::Type *bool_typ() const noexcept {
    return llvm::Type::getInt1Ty(m_context);
  }

  llvm::Type *i8_typ() const noexcept {
    return llvm::Type::getInt8Ty(m_context);
  }

  llvm::Type *i16_typ() const noexcept {
    return llvm::Type::getInt16Ty(m_context);
  }

  llvm::Type *i32_typ() const noexcept {
    return llvm::Type::getInt16Ty(m_context);
  }

  llvm::Type *i64_typ() const noexcept {
    return llvm::Type::getInt64Ty(m_context);
  }

  llvm::Type *f16_typ() const noexcept {
    return llvm::Type::getHalfTy(m_context);
  }

  llvm::Type *f32_typ() const noexcept {
    return llvm::Type::getFloatTy(m_context);
  }

  llvm::Type *f64_typ() const noexcept {
    return llvm::Type::getDoubleTy(m_context);
  }

  llvm::Value *i8val(int8_t val) const noexcept {
    return llvm::ConstantInt::get(i8_typ(), val, false);
  }

  llvm::Value *i16val(long val) const noexcept {
    return llvm::ConstantInt::get(i64_typ(), val, false);
  }

  llvm::Value *i32val(long val) const noexcept {
    return llvm::ConstantInt::get(i64_typ(), val, false);
  }

  llvm::Value *i64val(long val) const noexcept {
    return llvm::ConstantInt::get(i64_typ(), val, false);
  }

  llvm::Value *u8val(int8_t val) const noexcept {
    return llvm::ConstantInt::get(i8_typ(), val, true);
  }

  llvm::Value *u16val(long val) const noexcept {
    return llvm::ConstantInt::get(i64_typ(), val, true);
  }

  llvm::Value *u32val(long val) const noexcept {
    return llvm::ConstantInt::get(i64_typ(), val, true);
  }

  llvm::Value *u64val(long val) const noexcept {
    return llvm::ConstantInt::get(i64_typ(), val, true);
  }

  llvm::Value *f16val(long val) const noexcept {
    return llvm::ConstantFP::get(f16_typ(), val);
  }

  llvm::Value *f32val(long val) const noexcept {
    return llvm::ConstantFP::get(f32_typ(), val);
  }

  llvm::Value *f64val(long val) const noexcept {
    return llvm::ConstantFP::get(f64_typ(), val);
  }

  llvm::Value *boolval(bool val) const noexcept {
    return llvm::ConstantInt::get(bool_typ(), val, true);
  }

  llvm::Type *from_arrow(const arrow::DataType &type) const {
    switch (type.id()) {
    case arrow::Type::type::INT8:
    case arrow::Type::type::UINT8:
      return i8_typ();
    case arrow::Type::type::INT16:
    case arrow::Type::type::UINT16:
      return i16_typ();
    case arrow::Type::type::INT32:
    case arrow::Type::type::UINT32:
      return i32_typ();
    case arrow::Type::type::INT64:
    case arrow::Type::type::UINT64:
      return i64_typ();
    case arrow::Type::type::HALF_FLOAT:
      return f16_typ();
    case arrow::Type::type::FLOAT:
      return f32_typ();
    case arrow::Type::type::DOUBLE:
      return f64_typ();
    default:
      throw pefa::NotImplementedException(std::string("arrow ") + type.ToString() +
                                          " does not supported yet");
    }
  }

  llvm::Type *ptr_from_arrow(const arrow::DataType &type) const {
    return from_arrow(type)->getPointerTo();
  }

  llvm::Value *arrow_typed_const(const arrow::DataType &type, int64_t val) const {
    switch (type.id()) {
      PEFA_CASE_RET(PEFA_INT8_CASE, i8val(val))
      PEFA_CASE_RET(PEFA_INT16_CASE, i16val(val))
      PEFA_CASE_RET(PEFA_INT32_CASE, i32val(val))
      PEFA_CASE_RET(PEFA_INT64_CASE, i64val(val))
      PEFA_CASE_RET(PEFA_UINT8_CASE, u8val(val))
      PEFA_CASE_RET(PEFA_UINT16_CASE, u16val(val))
      PEFA_CASE_RET(PEFA_UINT32_CASE, u32val(val))
      PEFA_CASE_RET(PEFA_UINT64_CASE, u64val(val))
    }
    throw UnreachableException();
  }

  llvm::Value *arrow_typed_const(const arrow::DataType &type, double val) const {
    switch (type.id()) {
      PEFA_CASE_RET(PEFA_FLOAT16_CASE, f16val(val))
      PEFA_CASE_RET(PEFA_FLOAT32_CASE, f32val(val))
      PEFA_CASE_RET(PEFA_FLOAT64_CASE, f64val(val))
      PEFA_CASE_BRK(PEFA_DECIMAL_CASE,
                    throw NotImplementedException("Decimal support is not implemented yet"))
    }
    throw UnreachableException();
  }

#define __PEFA_CREATE_CMP_FUNC(name, icmp, ucmp, fcmp)                                             \
  llvm::Value *name(const arrow::DataType &type, llvm::IRBuilder<> &builder, llvm::Value *lhs,     \
                    llvm::Value *rhs) const {                                                      \
    switch (type.id()) {                                                                           \
      PEFA_CASE_RET(PEFA_SIGNED_INTEGRAL_CASE, builder.icmp(lhs, rhs))                             \
      PEFA_CASE_RET(PEFA_UNSIGNED_INTEGRAL_CASE, builder.ucmp(lhs, rhs))                           \
      PEFA_CASE_RET(PEFA_FLOATING_CASE, builder.fcmp(lhs, rhs))                                    \
    }                                                                                              \
    throw NotImplementedException(std::string("Comparing elements of type") + type.ToString() +    \
                                  " is not supported yet");                                        \
  }

  __PEFA_CREATE_CMP_FUNC(create_cmp_gt, CreateICmpSGT, CreateICmpUGT, CreateFCmpOGT)
  __PEFA_CREATE_CMP_FUNC(create_cmp_lt, CreateICmpSLT, CreateICmpULT, CreateFCmpOLT)
  __PEFA_CREATE_CMP_FUNC(create_cmp_ge, CreateICmpSGE, CreateICmpUGE, CreateFCmpOGE)
  __PEFA_CREATE_CMP_FUNC(create_cmp_le, CreateICmpSLE, CreateICmpULE, CreateFCmpOLE)
  __PEFA_CREATE_CMP_FUNC(create_cmp_eq, CreateICmpEQ, CreateICmpEQ, CreateFCmpOEQ)
  __PEFA_CREATE_CMP_FUNC(create_cmp_ne, CreateICmpNE, CreateICmpNE, CreateFCmpONE)

  template <typename Variant>
  llvm::Value *const_from_variant(const arrow::DataType &type, const Variant &variant) const {
    switch (type.id()) {
      PEFA_CASE_RET(PEFA_INTEGRAL_CASE, arrow_typed_const(type, std::get<int64_t>(variant)))
      PEFA_CASE_RET(PEFA_FLOATING_CASE PEFA_DECIMAL_CASE,
                    arrow_typed_const(type, std::get<double>(variant)))
    }
    throw UnreachableException();
  }
};
} // namespace pefa::internal::utils

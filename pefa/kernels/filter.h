#pragma once
#include "pefa/api/expressions.h"

#include <arrow/api.h>

namespace pefa::internal::kernels {
class FilterKernel {
public:
  virtual void execute(std::shared_ptr<arrow::Array>, std::shared_ptr<arrow::UInt8Array>) = 0;
  static std::unique_ptr<FilterKernel> create_cpu(std::shared_ptr<arrow::Field> field, std::shared_ptr<Expr> expr);
  virtual ~FilterKernel() = default;
};
} // namespace pefa::internal::kernels
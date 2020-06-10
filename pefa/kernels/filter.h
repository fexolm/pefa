#pragma once
#include "pefa/api/expressions.h"

#include <arrow/api.h>

namespace pefa::internal::kernels {

class FilterKernel {
public:
  // Filter kernel generates validity bitmap for array with ones on posiotions, where expr is true
  // It does not takes into account last <(size - offset) % 8> elements, and first <offset> elements
  // which should be processed separately to avoid data dependency between chunks

  virtual void execute(std::shared_ptr<arrow::Array>, uint8_t *bitmap, size_t offset) = 0;
  virtual void compile() = 0;
  static std::unique_ptr<FilterKernel> create_cpu(std::shared_ptr<arrow::Field> field,
                                                  std::shared_ptr<Expr> expr);
  virtual ~FilterKernel() = default;
};
} // namespace pefa::internal::kernels
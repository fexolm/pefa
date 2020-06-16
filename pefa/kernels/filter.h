#pragma once
#include "pefa/query_compiler/expressions.h"

#include <arrow/api.h>

namespace pefa::kernels {
using namespace query_compiler;
class FilterKernel {
public:
  // Filter kernel generates validity bitmap for array with ones on posiotions, where expr is true
  // It does not takes into account last <(size - offset) % 8> elements, and first <offset> elements
  // which should be processed separately to avoid data dependency between chunks

  virtual void execute(std::shared_ptr<const arrow::Array>, uint8_t *bitmap, size_t offset) = 0;
  virtual void execute_remaining(std::shared_ptr<const arrow::Array> column, uint8_t *bitmap,
                                 size_t array_offset, uint8_t bit_offset) = 0;
  virtual void compile() = 0;

  [[nodiscard]] static std::unique_ptr<FilterKernel>
  create_cpu(std::shared_ptr<const arrow::Field> field, std::shared_ptr<const Expr> expr);

  virtual ~FilterKernel() = default;
};
} // namespace pefa::kernels
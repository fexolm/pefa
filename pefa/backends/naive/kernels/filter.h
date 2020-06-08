#pragma once
#include "pefa/expressions/expressions.h"

#include <arrow/api.h>
namespace pefa::backends::naive::kernels {
void gen_filters(std::shared_ptr<arrow::Schema> schema, std::shared_ptr<internal::Expr> expr);
} // namespace pefa::backends::naive::kernels
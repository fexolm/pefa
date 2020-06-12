#pragma once
#include <arrow/type.h>
#include <cassert>

#define PEFA_CASE_BRK(condition, body)                                                             \
  condition {                                                                                      \
    body;                                                                                          \
    break;                                                                                         \
  }

#define PEFA_CASE_RET(condition, body)                                                             \
  condition {                                                                                      \
    return body;                                                                                   \
  }

#define PEFA_INT8_CASE    case arrow::Type::INT8:
#define PEFA_INT16_CASE   case arrow::Type::INT16:
#define PEFA_INT32_CASE   case arrow::Type::INT32:
#define PEFA_INT64_CASE   case arrow::Type::INT64:
#define PEFA_UINT8_CASE   case arrow::Type::UINT8:
#define PEFA_UINT16_CASE  case arrow::Type::UINT16:
#define PEFA_UINT32_CASE  case arrow::Type::UINT32:
#define PEFA_UINT64_CASE  case arrow::Type::UINT64:
#define PEFA_FLOAT16_CASE case arrow::Type::HALF_FLOAT:
#define PEFA_FLOAT32_CASE case arrow::Type::FLOAT:
#define PEFA_FLOAT64_CASE case arrow::Type::DOUBLE:
#define PEFA_DECIMAL_CASE case arrow::Type::DECIMAL:;

#define PEFA_SIGNED_INTEGRAL_CASE                                                                  \
  PEFA_INT8_CASE                                                                                   \
  PEFA_INT16_CASE                                                                                  \
  PEFA_INT32_CASE                                                                                  \
  PEFA_INT64_CASE

#define PEFA_UNSIGNED_INTEGRAL_CASE                                                                \
  PEFA_UINT8_CASE                                                                                  \
  PEFA_UINT16_CASE                                                                                 \
  PEFA_UINT32_CASE                                                                                 \
  PEFA_UINT64_CASE

#define PEFA_INTEGRAL_CASE                                                                         \
  PEFA_SIGNED_INTEGRAL_CASE                                                                        \
  PEFA_UNSIGNED_INTEGRAL_CASE

#define PEFA_FLOATING_CASE                                                                         \
  PEFA_FLOAT16_CASE                                                                                \
  PEFA_FLOAT32_CASE                                                                                \
  PEFA_FLOAT64_CASE

#define PEFA_NUMERIC_CASE                                                                          \
  PEFA_INTEGRAL_CASE                                                                               \
  PEFA_FLOATING_CASE

#include "pefa/kernels/filter.h"

#include <arrow/api.h>
#include <gtest/gtest.h>

using namespace pefa::internal;

class FilterKernelTest : public ::testing::Test {};

TEST_F(FilterKernelTest, testFilter) {
  std::vector<int64_t> values = {0, 4, 2, 4, 4, 5, 4, 7, 4, 9, 12, 4, 3};
  auto column_buffer = arrow::AllocateBuffer(sizeof(uint64_t) * values.size()).ValueOrDie();
  auto data = reinterpret_cast<int64_t *>(column_buffer->mutable_data());
  for (int i = 0; i < values.size(); i++) {
    data[i] = values[i];
  }

  auto bitmap_buffer = arrow::AllocateEmptyBitmap(values.size()).ValueOrDie();

  auto column = std::make_shared<arrow::Int64Array>(values.size(), std::shared_ptr(std::move(column_buffer)));
  auto bitmap = std::make_shared<arrow::UInt8Array>(values.size() / 8 + 1, bitmap_buffer);

  auto field = std::make_shared<arrow::Field>("A", arrow::int64());
  auto expr = ((pefa::col("A")->EQ(pefa::lit(4)))->AND(pefa::col("A")->GE(pefa::lit(3))))->OR(pefa::col("B")->EQ(pefa::lit(1)));

  auto filter = kernels::FilterKernel::create_cpu(field, expr);

  filter->execute(column, bitmap);

  ASSERT_EQ(bitmap_buffer->data()[0], 0b01011010);
  ASSERT_EQ(bitmap_buffer->data()[1], 0b10010000);

}

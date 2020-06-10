#include "pefa/kernels/filter.h"

#include <arrow/api.h>
#include <arrow/testing/gtest_util.h>
#include <gtest/gtest.h>

using namespace pefa::internal;

class FilterKernelTest : public ::testing::Test {
protected:
  std::shared_ptr<arrow::Buffer> m_bitmap;
  std::shared_ptr<arrow::Array> m_array;
  std::shared_ptr<arrow::Field> m_field;
  void SetUp() override {
    m_array = arrow::ArrayFromJSON(arrow::int64(), "[0, 4, 2, 4, 4, 5, 4, 7, 4, 9, 12, 4, 3]");
    m_bitmap = allocateBitmap(m_array->length());
    m_field = std::make_shared<arrow::Field>("field", arrow::int64());
  }

private:
  std::shared_ptr<arrow::Buffer> allocateBitmap(size_t length) {
    auto bitmap = arrow::AllocateEmptyBitmap(length).ValueOrDie();
    std::memset(bitmap->mutable_data(), 255, bitmap->size());
    return bitmap;
  }
};

class FilterKernelOffsetsTest : public FilterKernelTest {
protected:
  std::unique_ptr<kernels::FilterKernel> m_filter;
  void SetUp() override {
    FilterKernelTest::SetUp();
    auto expr = ((pefa::col("field")->EQ(pefa::lit(4)))->AND(pefa::col("field")->GE(pefa::lit(3))))
                    ->OR(pefa::col("smthelse")->EQ(pefa::lit(1)));
    m_filter = kernels::FilterKernel::create_cpu(m_field, expr);
    m_filter->compile();
  }
};

TEST_F(FilterKernelOffsetsTest, testWithoutOffset) {
  m_filter->execute(m_array, m_bitmap->mutable_data(), 0);
  ASSERT_EQ(m_bitmap->data()[0], 0b01011010);
  // we don't filter last byte with that kernel
  ASSERT_EQ(m_bitmap->data()[1], 0b11111111);
}

TEST_F(FilterKernelOffsetsTest, testWithOffset) {
  m_filter->execute(m_array, m_bitmap->mutable_data(), 2);
  // we expect first value to be shared between 2 chunks, so
  ASSERT_EQ(m_bitmap->data()[0], 0b11111111);
  ASSERT_EQ(m_bitmap->data()[1], 0b01101010);
}

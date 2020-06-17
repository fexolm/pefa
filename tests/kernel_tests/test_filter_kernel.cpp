#include "pefa/kernels/filter.h"

#include <arrow/api.h>
#include <arrow/testing/gtest_util.h>
#include <arrow/type_traits.h>
#include <gtest/gtest.h>

using namespace pefa;
using namespace pefa::query_compiler;

template <typename ArrowType>
class FilterKernelTest : public ::testing::Test {
protected:
  std::shared_ptr<arrow::Buffer> m_bitmap;
  std::shared_ptr<arrow::Array> m_array;
  std::shared_ptr<arrow::Field> m_field;

  void SetUp() override {
    auto type = arrow::TypeTraits<ArrowType>::type_singleton();
    m_array = arrow::ArrayFromJSON(type, "[0, 4, 2, 4, 4, 5, 4, 7, 4, 9, 12, 4, 3]");
    m_bitmap = allocateBitmap(m_array->length());
    m_field = std::make_shared<arrow::Field>("field", type);
  }

private:
  std::shared_ptr<arrow::Buffer> allocateBitmap(size_t length) {
    auto bitmap = arrow::AllocateEmptyBitmap(length).ValueOrDie();
    std::memset(bitmap->mutable_data(), 255, bitmap->size());
    return bitmap;
  }
};

template <typename ArrowType>
class FilterKernelOffsetsTest : public FilterKernelTest<ArrowType> {
protected:
  std::unique_ptr<kernels::FilterKernel> m_filter;

  void SetUp() override {
    FilterKernelTest<ArrowType>::SetUp();
    auto expr = ((col("field")->EQ(lit(4)))->AND(col("field")->GE(lit(3))));
    m_filter = kernels::FilterKernel::create_cpu(this->m_field, expr);
    m_filter->compile();
  }
};

TYPED_TEST_SUITE(FilterKernelOffsetsTest, arrow::NumericArrowTypes);
TYPED_TEST(FilterKernelOffsetsTest, testWithoutOffset) {
  // skip last bitmap in buffer, as it should be processed separately
  this->m_filter->execute(this->m_array, this->m_bitmap->mutable_data(), 0);
  arrow::AssertBufferEqual(*this->m_bitmap, std::vector<uint8_t>({0b01011010, 0b11111111}));
}

TYPED_TEST(FilterKernelOffsetsTest, testWithOffset2) {
  // skip first bitmap in buffer, as it is assumed that it is shared between 2 chunks
  this->m_filter->execute(this->m_array, this->m_bitmap->mutable_data(), 2);
  arrow::AssertBufferEqual(*this->m_bitmap, std::vector<uint8_t>({0b11111111, 0b01101010}));
}

TYPED_TEST(FilterKernelOffsetsTest, testWithOffset4) {
  // skip first bitmap in buffer, as it is assumed that it is shared between 2 chunks
  this->m_filter->execute(this->m_array, this->m_bitmap->mutable_data(), 4);
  arrow::AssertBufferEqual(*this->m_bitmap, std::vector<uint8_t>({0b11111111, 0b10101001}));
}

TYPED_TEST(FilterKernelOffsetsTest, testWithOffset10) {
  // skip all bitmaps, as it is assumed, that 2 bitmaps should be skipped
  this->m_filter->execute(this->m_array, this->m_bitmap->mutable_data(), 10);
  arrow::AssertBufferEqual(*this->m_bitmap, std::vector<uint8_t>({0b11111111, 0b11111111}));
}

TYPED_TEST(FilterKernelOffsetsTest, testRemainingBeginOffset2) {
  this->m_filter->execute_remaining(this->m_array, this->m_bitmap->mutable_data(), 0, 2);
  arrow::AssertBufferEqual(*this->m_bitmap, std::vector<uint8_t>({0b11010110, 0b11111111}));
}

TYPED_TEST(FilterKernelOffsetsTest, testRemainingBeginOffset4) {
  this->m_filter->execute_remaining(this->m_array, this->m_bitmap->mutable_data(), 0, 4);
  arrow::AssertBufferEqual(*this->m_bitmap, std::vector<uint8_t>({0b11110101, 0b11111111}));
}

TYPED_TEST(FilterKernelOffsetsTest, testRemainingEndOffset2) {
  this->m_filter->execute_remaining(this->m_array, this->m_bitmap->mutable_data(), 0, 4);
  arrow::AssertBufferEqual(*this->m_bitmap, std::vector<uint8_t>({0b11110101, 0b11111111}));
}

TYPED_TEST(FilterKernelOffsetsTest, testRemainingEnd2) {
  this->m_filter->execute_remaining(this->m_array, this->m_bitmap->mutable_data(),
                                    this->m_array->length() - 2, 0);
  arrow::AssertBufferEqual(*this->m_bitmap, std::vector<uint8_t>({0b10111111, 0b11111111}));
}

TYPED_TEST(FilterKernelOffsetsTest, testRemainingEnd4) {
  this->m_filter->execute_remaining(this->m_array, this->m_bitmap->mutable_data(),
                                    this->m_array->length() - 4, 0);
  arrow::AssertBufferEqual(*this->m_bitmap, std::vector<uint8_t>({0b00101111, 0b11111111}));
}

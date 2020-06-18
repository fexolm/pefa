#include "../utils.h"

#include <arrow/api.h>
#include <arrow/testing/gtest_util.h>
#include <arrow/testing/random.h>
#include <gtest/gtest.h>
#include <memory>
#include <pefa/execution/execution.h>
#include <pefa/query_compiler/query_compiler.h>

class FilterExecutorTest : public ::testing::Test {
protected:
  std::shared_ptr<arrow::Table> m_table;

public:
  void SetUp() override {
    auto schema = std::make_shared<arrow::Schema>(std::vector<std::shared_ptr<arrow::Field>>{
        std::make_shared<arrow::Field>("A", arrow::int32()),
        std::make_shared<arrow::Field>("B", arrow::int16()),
        std::make_shared<arrow::Field>("C", arrow::float64())});

    std::vector<std::shared_ptr<arrow::ChunkedArray>> columns{
        arrow::ChunkedArrayFromJSON(arrow::int32(), {"[1,2,3,8,12,3,12,534,123,53,3,12,3,532,1,32]",
                                                     "[2,12,23,3,43,54,65,23,43,3,4,2,1,6,2]",
                                                     "[4,3,2,4,6,5,23,4,6,2,1,3,2,1,2,3]"}),
        arrow::ChunkedArrayFromJSON(
            arrow::int16(), {"[1,5,1,2,5,1,3,6,1]", "[2,4,1,5,7,1,2,56,671,2,5,2,3,56]",
                             "[3,2,6,6,1,2,6,778,12,2,6,1,2,6,3]", "[3,3,56,2,2,36,7,2,3]"}),

        arrow::ChunkedArrayFromJSON(
            arrow::float64(),
            {"[2.1, 2.56, 1.2, 634.12, 2.3, 363.6, 21.3, 332.65, 2.32, 453.34, 323.432]",
             "[43.235, 34.22, 43.6, 43.23, 65.34, 653.45, 5.3, 65.7, 543.2, 435.6, 2345.2]",
             "[3.2, 43.2, 325.6, 32.5, 323.7, 32.7, 323.6, 32.4, 31.3, 325.1, 323.7, 3223.5]",
             "[5.4, 32.5, 223.7, 32.4, 327.2, 32.7, 23.5, 323,6, 12.8, 12.8, 23.56, 14.7]"})};

    m_table = arrow::Table::Make(schema, columns);
  }
};

namespace pefa::execution {
std::shared_ptr<arrow::Buffer> generate_filter_bitmap(const std::shared_ptr<ExecutionContext> &ctx,
                                                      const std::shared_ptr<BooleanExpr> &expr);
}

TEST_F(FilterExecutorTest, testFilterBitmapGeneration) {
  auto ctx = std::make_shared<pefa::execution::ExecutionContext>(m_table);
  using namespace pefa::query_compiler;
  auto expr = (col("A")->GE(lit(10))->AND(col("B")->LE(lit(7)))->AND(col("C")->GE(lit(5.0))));
  auto bitmap = pefa::execution::generate_filter_bitmap(ctx, expr);
  ASSERT_EQ(bitmap->size(), 6);

  std::vector<bool> a_res(m_table->column(0)->length(), true);
  std::vector<bool> b_res(m_table->column(1)->length(), true);
  std::vector<bool> c_res(m_table->column(2)->length(), true);

  ASSERT_EQ(a_res.size(), 47);
  ASSERT_EQ(b_res.size(), 47);
  ASSERT_EQ(c_res.size(), 47);

  size_t offset = 0;
  for (int chunk_num = 0; chunk_num < m_table->column(0)->num_chunks(); chunk_num++) {
    auto chunk = m_table->column(0)->chunk(chunk_num);
    for (int i = 0; i < chunk->length(); i++) {
      auto array = std::static_pointer_cast<arrow::Int32Array>(chunk);
      a_res[offset + i] = array->Value(i) >= 10;
    }
    offset += chunk->length();
  }

  offset = 0;
  for (int chunk_num = 0; chunk_num < m_table->column(1)->num_chunks(); chunk_num++) {
    auto chunk = m_table->column(1)->chunk(chunk_num);
    for (int i = 0; i < chunk->length(); i++) {
      auto array = std::static_pointer_cast<arrow::Int16Array>(chunk);
      b_res[offset + i] = array->Value(i) <= 7;
    }
    offset += chunk->length();
  }

  offset = 0;
  for (int chunk_num = 0; chunk_num < m_table->column(2)->num_chunks(); chunk_num++) {
    auto chunk = m_table->column(2)->chunk(chunk_num);
    for (int i = 0; i < chunk->length(); i++) {
      auto array = std::static_pointer_cast<arrow::DoubleArray>(chunk);
      c_res[offset + i] = array->Value(i) >= 5.0;
    }
    offset += chunk->length();
  }

  for (int i = 0; i < 6; i++) {
    for (int j = 0; j < 8; j++) {
      if (i * 8 + j >= 47) {
        break;
      }
      ASSERT_EQ(a_res[i * 8 + j] && b_res[i * 8 + j] && c_res[i * 8 + j],
                bitmap->data()[i] >> (7 - j) & 1);
    }
  }
}

class FilterEndToEndTest : public ::testing::Test {
protected:
  std::shared_ptr<arrow::Table> m_table;

public:
  void SetUp() override {
    m_table = read_arrow_table("chicago_taxi_trips_2016_01.csv");
  }
};

TEST_F(FilterEndToEndTest, filterIdTest) {
  using namespace pefa::query_compiler;
  QueryCompiler qc(m_table);
  ASSERT_EQ(qc.project({"taxi_id"}).filter(col("taxi_id")->EQ(lit(523))).execute()->num_rows(),
            406);

  ASSERT_EQ(qc.project({"taxi_id"}).filter(col("taxi_id")->EQ(lit(345))).execute()->num_rows(),
            102);

  ASSERT_EQ(qc.project({"taxi_id"}).filter(col("taxi_id")->EQ(lit(6345))).execute()->num_rows(),
            83);

  ASSERT_EQ(qc.project({"taxi_id"}).filter(col("taxi_id")->EQ(lit(52341))).execute()->num_rows(),
            0);

  ASSERT_EQ(qc.project({"taxi_id"}).filter(col("taxi_id")->LE(lit(12))).execute()->num_rows(),
            3573);

  ASSERT_EQ(qc.project({"taxi_id"}).filter(col("taxi_id")->LE(lit(70))).execute()->num_rows(),
            16276);

  ASSERT_EQ(qc.project({"taxi_id"}).filter(col("taxi_id")->LE(lit(75))).execute()->num_rows(),
            17495);

  ASSERT_EQ(qc.project({"taxi_id"}).filter(col("taxi_id")->LE(lit(1000))).execute()->num_rows(),
            192525);
}

TEST_F(FilterEndToEndTest, complexFilteringTest) {
  using namespace pefa::query_compiler;
  QueryCompiler qc(m_table);
  ASSERT_EQ(qc.project({"taxi_id", "trip_seconds"})
                .filter((col("taxi_id")->LE(lit(1000)))->AND(col("trip_seconds")->LT(lit(20))))
                .execute()
                ->num_rows(),
            18571);

  ASSERT_EQ(qc.project({"taxi_id", "trip_seconds", "trip_miles"})
                .filter((col("taxi_id")->LE(lit(1000)))->OR((col("trip_seconds")->LT(lit(20)))))
                .execute()
                ->num_rows(),
            362588);

  ASSERT_EQ(
      qc.project({"taxi_id", "trip_seconds", "trip_miles"})
          .filter(
              (col("taxi_id")->LE(lit(1000)))
                  ->AND((col("trip_seconds")->LT(lit(20)))->OR(col("trip_miles")->LT(lit(0.7)))))
          .execute()
          ->num_rows(),
      74343);
}
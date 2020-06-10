#include <arrow/csv/api.h>
#include <arrow/io/api.h>
#include <arrow/memory_pool.h>
#include <arrow/table.h>
#include <gtest/gtest.h>
#include <memory>
#include <pefa/api/api.h>
#include <pefa/backends/naive/backend.h>
#include <pefa/jit/jit.h>
#include <pefa/kernels/filter.h>

class NotSegfaultTest : public ::testing::Test {};

TEST_F(NotSegfaultTest, projectionTest) {
  auto memory_pool = arrow::default_memory_pool();
  auto arrow_parse_options = arrow::csv::ParseOptions::Defaults();
  auto arrow_read_options = arrow::csv::ReadOptions::Defaults();
  auto arrow_convert_options = arrow::csv::ConvertOptions::Defaults();

  auto file_result = arrow::io::ReadableFile::Open(TEST_DATA_DIR "/projection_test.csv");
  auto inp = file_result.ValueOrDie();

  auto table_reader = arrow::csv::TableReader::Make(memory_pool, inp, arrow_read_options,
                                                    arrow_parse_options, arrow_convert_options)
                          .ValueOrDie();

  auto table = table_reader->Read().ValueOrDie();

  pefa::ExecutionContext<pefa::backends::naive::Backend> ctx(table);

  auto result = ctx.project({"a", "b"}).execute();
}

TEST_F(NotSegfaultTest, filterTest) {
  auto memory_pool = arrow::default_memory_pool();
  auto arrow_parse_options = arrow::csv::ParseOptions::Defaults();
  auto arrow_read_options = arrow::csv::ReadOptions::Defaults();
  auto arrow_convert_options = arrow::csv::ConvertOptions::Defaults();

  auto file_result = arrow::io::ReadableFile::Open(TEST_DATA_DIR "/projection_test.csv");
  auto inp = file_result.ValueOrDie();

  auto table_reader = arrow::csv::TableReader::Make(memory_pool, inp, arrow_read_options,
                                                    arrow_parse_options, arrow_convert_options)
                          .ValueOrDie();

  auto table = table_reader->Read().ValueOrDie();

  pefa::ExecutionContext<pefa::backends::naive::Backend> ctx(table);

  auto result = ctx.project({"a", "b"});

  auto expr = ((pefa::col("a")->EQ(pefa::lit(4)))->AND(pefa::col("a")->GE(pefa::lit(3))))
                  ->OR(pefa::col("b")->EQ(pefa::lit(1)));

  result = result.filter(expr);

  result.execute();
}
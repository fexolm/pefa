#include <arrow/csv/api.h>
#include <arrow/io/api.h>
#include <arrow/memory_pool.h>
#include <arrow/table.h>
#include <gtest/gtest.h>
#include <memory>
#include <pefa/query_compiler/expressions.h>
#include <pefa/query_compiler/query_compiler.h>

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

  pefa::query_compiler::QueryCompiler qc(table);

  auto result = qc.project({"a", "b"}).execute();
}

TEST_F(NotSegfaultTest, filterTest) {
  using namespace pefa::query_compiler;

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

  pefa::query_compiler::QueryCompiler qc(table);

  auto expr = ((col("a")->GE(lit(0)))->AND(col("a")->GE(lit(3))))->OR(col("b")->EQ(lit(1)));

  auto result = qc.filter(expr).execute();
}
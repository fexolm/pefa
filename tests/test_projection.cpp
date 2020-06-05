#include <arrow/csv/api.h>
#include <arrow/io/api.h>
#include <arrow/memory_pool.h>
#include <arrow/table.h>
#include <gtest/gtest.h>
#include <memory>
#include <pefa/api.h>
#include <pefa/backends/naive/backend.h>

class ProjectionTest : public ::testing::Test {};

TEST_F(ProjectionTest, projectionTest) {
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
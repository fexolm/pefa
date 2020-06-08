#include <arrow/csv/api.h>
#include <arrow/io/api.h>
#include <arrow/memory_pool.h>
#include <arrow/table.h>
#include <gtest/gtest.h>
#include <memory>
#include <pefa/api.h>
#include <pefa/backends/naive/backend.h>
#include <pefa/backends/naive/kernels/filter.h>
#include <pefa/jit/jit.h>

class NotSegfaultTest : public ::testing::Test {};

TEST_F(NotSegfaultTest, projectionTest) {
  auto memory_pool = arrow::default_memory_pool();
  auto arrow_parse_options = arrow::csv::ParseOptions::Defaults();
  auto arrow_read_options = arrow::csv::ReadOptions::Defaults();
  auto arrow_convert_options = arrow::csv::ConvertOptions::Defaults();

  auto file_result = arrow::io::ReadableFile::Open(TEST_DATA_DIR "/projection_test.csv");
  auto inp = file_result.ValueOrDie();

  auto table_reader =
      arrow::csv::TableReader::Make(memory_pool, inp, arrow_read_options, arrow_parse_options, arrow_convert_options).ValueOrDie();

  auto table = table_reader->Read().ValueOrDie();

  pefa::ExecutionContext<pefa::backends::naive::Backend> ctx(table);

  auto result = ctx.project({"a", "b"}).execute();

  pefa::backends::naive::kernels::gen_filters(
      result->schema(), ((pefa::col("a")->EQ(pefa::lit(4)))->AND(pefa::col("a")->GE(pefa::lit(3))))->OR(pefa::col("b")->EQ(pefa::lit(1))));

  auto sym = pefa::jit::get_JIT()->findSymbol("a_filter");
  auto f = (void (*)(int8_t *, int8_t *, int64_t))sym.getAddress().get();
  int64_t a[] = {1, 2, 3, 4, 5, 6, 7, 8};
  int8_t b[] = {0};
  f((int8_t *)a, b, 8);
  ASSERT_EQ(b[0], 0b00010000);
}

TEST_F(NotSegfaultTest, expressionsTest) {
  using namespace pefa;
  auto expr1 = ((col("a")->EQ(lit(5)))->OR((col("a")->LE(lit(6)))->AND(col("c")->NEQ(lit("abc")))));
}
#include "utils.h"

#include <gtest/gtest.h>
#include <memory>
#include <pefa/query_compiler/expressions.h>
#include <pefa/query_compiler/query_compiler.h>

class NotSegfaultTest : public ::testing::Test {};

TEST_F(NotSegfaultTest, projectionTest) {
  auto table = read_arrow_table("/projection_test.csv");
  pefa::query_compiler::QueryCompiler qc(table);
  auto result = qc.project({"a", "b"}).execute();
}

TEST_F(NotSegfaultTest, filterTest) {
  using namespace pefa::query_compiler;

  auto table = read_arrow_table("/projection_test.csv");
  pefa::query_compiler::QueryCompiler qc(table);
  auto expr = ((col("a")->GE(lit(0)))->AND(col("a")->GE(lit(3))))->OR(col("b")->EQ(lit(1)));
  auto result = qc.filter(expr).execute();
}
#include <arrow/api.h>
#include <arrow/testing/random.h>
#include <benchmark/benchmark.h>
#include <memory>
#include <pefa/kernels/filter.h>

using namespace pefa;
using namespace pefa::query_compiler;
class FilterKernelBenchmarkFixture : public benchmark::Fixture {
protected:
  std::unique_ptr<kernels::FilterKernel> m_filter;
  std::shared_ptr<arrow::DataType> m_type;

public:
  void SetUp(const ::benchmark::State &state) override {
    m_type = arrow::int16();

    auto field = std::make_shared<arrow::Field>("field", arrow::int16());
    auto expr = (col("field")->EQ(lit(4)))->OR(col("field")->GT(lit(15)));
    m_filter = kernels::FilterKernel::create_cpu(field, expr);
    m_filter->compile();
  }
};

BENCHMARK_DEFINE_F(FilterKernelBenchmarkFixture, BenchmarkFilter)
(benchmark::State &state) {
  arrow::random::RandomArrayGenerator generator(152);
  auto array = generator.Int16(state.range(0), 0, 1400);
  auto bitmap = arrow::AllocateBitmap(state.range(0)).ValueOrDie();
  std::memset(bitmap->mutable_data(), 255, bitmap->size());
  auto &filter = *m_filter;
  for (auto _ : state) {
    filter.execute(array, bitmap->mutable_data(), 0);
  }
}

BENCHMARK_REGISTER_F(FilterKernelBenchmarkFixture, BenchmarkFilter)
    ->RangeMultiplier(100)
    ->Range(1000, 100000000);

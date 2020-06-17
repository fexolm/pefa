#include "execution.h"

#include "execution_context.h"
#include "pefa/kernels/filter.h"

#include <arrow/api.h>
#include <memory>
#include <utility>

namespace pefa::execution {
std::shared_ptr<ExecutionContext> project(const std::shared_ptr<ExecutionContext> &ctx,
                                          std::vector<std::string> column_names) {
  std::vector<std::shared_ptr<arrow::ChunkedArray>> columns(column_names.size());
  std::vector<std::shared_ptr<arrow::Field>> fields(column_names.size());
  for (int i = 0; i < column_names.size(); i++) {
    auto col = ctx->table->GetColumnByName(column_names[i]);
    fields[i] = std::make_shared<arrow::Field>(column_names[i], col->type());
    columns[i] = col;
  }
  return std::make_shared<ExecutionContext>(
      arrow::Table::Make(std::make_shared<arrow::Schema>(fields), columns));
}

class FilterExprExecutor : public ExprVisitor {
private:
  std::shared_ptr<ExecutionContext> m_ctx;
  std::shared_ptr<arrow::Buffer> m_buffer;

public:
  explicit FilterExprExecutor(std::shared_ptr<ExecutionContext> ctx)
      : m_ctx(std::move(ctx)) {}
  void visit(const PredicateExpr &expr) override {
    expr.lhs->visit(*this);
    auto lhs_buf = m_buffer;
    expr.rhs->visit(*this);
    auto rhs_buf = m_buffer;
    if (expr.op == PredicateExpr::Op::AND) {
      for (int i = 0; i < lhs_buf->size(); i++) {
        lhs_buf->mutable_data()[i] &= rhs_buf->mutable_data()[i];
      }
    } else {
      for (int i = 0; i < lhs_buf->size(); i++) {
        lhs_buf->mutable_data()[i] |= rhs_buf->mutable_data()[i];
      }
    }
    m_buffer = lhs_buf;
  }
  void visit(const CompareExpr &expr) override {
    // TODO: support expressions like a > b, 3 * a > 5 * b
    // currently we support only expressions like a > <const>
    auto buffer = arrow::AllocateBitmap(m_ctx->table->num_rows()).ValueOrDie();
    std::memset(buffer->mutable_data(), 255, buffer->size());
    auto field = m_ctx->table->schema()->GetFieldByName(expr.lhs->name);
    auto column = m_ctx->table->GetColumnByName(expr.lhs->name);
    auto kernel = kernels::FilterKernel::create_cpu(field, std::make_shared<CompareExpr>(expr));
    kernel->compile();
    // TODO: make that implementation parallel
    for (int chunk_num = 0; chunk_num < column->num_chunks(); chunk_num++) {
      size_t offset = 0;
      for (int i = 0; i < chunk_num; i++) {
        offset += column->chunk(i)->length();
      }
      // if some byte from bitmap is located between 2 chunks, we calculate how much bits from that
      // byte belongs to previous chunk
      auto prev_bits = offset % 8;
      // this way we can calculate how much bits from that byte belons to current chunk
      // we use % 8 to handle case, when byte completely lies in current chunk, then prev_bits would
      // be equal to 0 and we don't need an offset
      auto remaining_bits = (8 - prev_bits) % 8;
      kernel->execute(column->chunk(chunk_num), buffer->mutable_data() + offset / 8,
                      remaining_bits);
    }
    size_t offset = 0;
    for (int chunk_num = 0; chunk_num < column->num_chunks(); chunk_num++) {
      if (offset % 8 != 0) {
        kernel->execute_remaining(column->chunk(chunk_num), buffer->mutable_data() + offset / 8, 0,
                                  offset % 8);
      }
      offset += column->chunk(chunk_num)->length();
      if (offset % 8 != 0) {
        kernel->execute_remaining(column->chunk(chunk_num), buffer->mutable_data() + offset / 8,
                                  column->chunk(chunk_num)->length() - (offset % 8), 0);
      }
    }
    m_buffer = buffer;
  }

  [[nodiscard]] std::shared_ptr<arrow::Buffer> result() const {
    return m_buffer;
  }
};

std::shared_ptr<arrow::Buffer> generate_filter_bitmap(const std::shared_ptr<ExecutionContext> &ctx,
                                                      const std::shared_ptr<BooleanExpr> &expr) {
  FilterExprExecutor expr_executor(ctx);
  expr->visit(expr_executor);
  return expr_executor.result();
}

std::shared_ptr<ExecutionContext> filter(const std::shared_ptr<ExecutionContext> &ctx,
                                         const std::shared_ptr<BooleanExpr> &expr) {
  if (ctx->table->schema()->num_fields() == 0 || ctx->table->column(0)->num_chunks() == 0) {
    return std::make_shared<ExecutionContext>(ctx->table, ctx->plan);
  }
  auto bitmap = generate_filter_bitmap(ctx, expr);
  // TODO: apply filter to all columns
  // As a solution for this we can calculate length of all arrays by using popcnt
  // Then allocate blocks with that lengths (merge small blocks if any)
  // And create chunks in parallel
  // Very big chunks can be split later by slicing

  // TODO: return generated table context
  return std::make_shared<ExecutionContext>(ctx->table, ctx->plan);
}
} // namespace pefa::execution

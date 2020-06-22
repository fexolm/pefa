#pragma once
#include "pefa/utils/exceptions.h"
#include "pefa/utils/utils.h"

#include <arrow/table.h>
#include <memory>
namespace pefa::execution {
struct ChunkMetadata {
  virtual ~ChunkMetadata() = default;
};

struct NullChunkMetadata : ChunkMetadata {};

template <typename T>
struct TypedChunkMetadata : ChunkMetadata {
  T min;
  T max;
};

struct ColumnMetadata {
  std::vector<std::unique_ptr<ChunkMetadata>> chunks;
  explicit ColumnMetadata(std::vector<std::unique_ptr<ChunkMetadata>> &&chunks);
};

struct TableMetadata {
  std::vector<std::shared_ptr<ColumnMetadata>> columns;
  std::shared_ptr<arrow::Buffer> filter_bitmap;
};

struct ExecutionContext {
  std::shared_ptr<arrow::Table> table;
  std::shared_ptr<TableMetadata> metadata;
  explicit ExecutionContext(std::shared_ptr<arrow::Table> table);
};
} // namespace pefa::execution
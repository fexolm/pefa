#include "execution_context.h"
namespace pefa::execution {
std::unique_ptr<ChunkMetadata> create_empty_chunk_metadata(const arrow::DataType &typ) {
  switch (typ.id()) {
  case arrow::Type::UINT8:
    return std::make_unique<TypedChunkMetadata<uint8_t>>();
  case arrow::Type::INT8:
    return std::make_unique<TypedChunkMetadata<int8_t>>();
  case arrow::Type::UINT16:
    return std::make_unique<TypedChunkMetadata<uint16_t>>();
  case arrow::Type::INT16:
    return std::make_unique<TypedChunkMetadata<int16_t>>();
  case arrow::Type::UINT32:
    return std::make_unique<TypedChunkMetadata<uint32_t>>();
  case arrow::Type::INT32:
    return std::make_unique<TypedChunkMetadata<int32_t>>();
  case arrow::Type::UINT64:
    return std::make_unique<TypedChunkMetadata<uint64_t>>();
  case arrow::Type::INT64:
    return std::make_unique<TypedChunkMetadata<int64_t>>();
  case arrow::Type::FLOAT:
    return std::make_unique<TypedChunkMetadata<float>>();
  case arrow::Type::DOUBLE:
    return std::make_unique<TypedChunkMetadata<double>>();
  case arrow::Type::STRING:
    return std::make_unique<TypedChunkMetadata<std::string>>();
  case arrow::Type::BOOL:
    return std::make_unique<TypedChunkMetadata<bool>>();
  case arrow::Type::NA:
    return std::make_unique<NullChunkMetadata>();
  case arrow::Type::HALF_FLOAT:
  case arrow::Type::BINARY:
  case arrow::Type::FIXED_SIZE_BINARY:
  case arrow::Type::DATE32:
  case arrow::Type::DATE64:
  case arrow::Type::TIMESTAMP:
  case arrow::Type::TIME32:
  case arrow::Type::TIME64:
  case arrow::Type::INTERVAL:
  case arrow::Type::DECIMAL:
  case arrow::Type::LIST:
  case arrow::Type::STRUCT:
  case arrow::Type::UNION:
  case arrow::Type::DICTIONARY:
  case arrow::Type::MAP:
  case arrow::Type::EXTENSION:
  case arrow::Type::FIXED_SIZE_LIST:
  case arrow::Type::DURATION:
  case arrow::Type::LARGE_STRING:
  case arrow::Type::LARGE_BINARY:
  case arrow::Type::LARGE_LIST:
    throw NotImplementedException(typ.ToString() + " type does not supported yet");
  }
  return nullptr; // unreachable
}
ExecutionContext::ExecutionContext(std::shared_ptr<arrow::Table> _table)
    : table(std::move(_table)) {
  metadata = std::make_shared<TableMetadata>();
  metadata->filter_bitmap = nullptr;
  for (int i = 0; i < table->num_columns(); i++) {
    auto &col = *table->column(i);
    std::vector<std::unique_ptr<ChunkMetadata>> chunks;
    chunks.reserve(col.num_chunks());
    for (int j = 0; j < col.num_chunks(); j++) {
      chunks.emplace_back(create_empty_chunk_metadata(*col.type()));
    }
    metadata->columns.emplace_back(std::make_shared<ColumnMetadata>(std::move(chunks)));
  }
}
ColumnMetadata::ColumnMetadata(std::vector<std::unique_ptr<ChunkMetadata>> &&chunks)
    : chunks(std::move(chunks)) {}
} // namespace pefa::execution

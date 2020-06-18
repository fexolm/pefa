#pragma once
#include <arrow/api.h>
#include <arrow/csv/api.h>
#include <arrow/io/api.h>
#include <arrow/memory_pool.h>
#include <memory>
#include <string>
std::shared_ptr<arrow::Table> read_arrow_table(std::string filename) {
  auto memory_pool = arrow::default_memory_pool();
  auto arrow_parse_options = arrow::csv::ParseOptions::Defaults();
  auto arrow_read_options = arrow::csv::ReadOptions::Defaults();
  auto arrow_convert_options = arrow::csv::ConvertOptions::Defaults();

  auto file_result = arrow::io::ReadableFile::Open(TEST_DATA_DIR + filename);
  auto inp = file_result.ValueOrDie();

  auto table_reader = arrow::csv::TableReader::Make(memory_pool, inp, arrow_read_options,
                                                    arrow_parse_options, arrow_convert_options)
                          .ValueOrDie();

  auto table = table_reader->Read().ValueOrDie();
  return table;
}
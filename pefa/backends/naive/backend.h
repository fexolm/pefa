#pragma once
#include "context.h"

namespace pefa::backends::naive {
struct Backend {
  typedef Context ContextType;

  static std::shared_ptr<Context> project(std::shared_ptr<Context> ctx,
                                          std::vector<std::string> columns);

  static std::shared_ptr<arrow::Table> execute(std::shared_ptr<Context> ctx);
};
} // namespace pefa::backends::naive
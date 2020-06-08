#pragma once

namespace pefa::backends {
template <typename Backend> struct BackendTraits { using ContextType = typename Backend::ContextType; };
} // namespace pefa::backends
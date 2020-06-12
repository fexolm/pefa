#include "exceptions.h"

pefa::BaseException::BaseException(std::string msg)
    : m_msg(msg) {}

const char *pefa::BaseException::what() const noexcept {
  return m_msg.c_str();
}

pefa::NotImplementedException::NotImplementedException(std::string msg)
    : BaseException(msg) {}

pefa::KernelNotCompiledException::KernelNotCompiledException()
    : BaseException("Kernel must be compiled before execution") {}

pefa::UnreachableException::UnreachableException()
    : BaseException("Unreachable branch invoked") {}

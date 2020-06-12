#pragma once
#include <exception>
#include <sstream>
namespace pefa {

class BaseException : public std::exception {
private:
  std::string m_msg;

public:
  explicit BaseException(std::string msg);

  virtual const char *what() const noexcept;
};

class NotImplementedException : public BaseException {
public:
  explicit NotImplementedException(std::string msg);
};

class KernelNotCompiledException : public BaseException {
public:
  KernelNotCompiledException();
};

class UnreachableException : public BaseException {
public:
  UnreachableException();
};
} // namespace pefa

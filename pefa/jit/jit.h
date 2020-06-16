#pragma once
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/ExecutionEngine/Orc/Core.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/IRTransformLayer.h>
#include <llvm/ExecutionEngine/Orc/Legacy.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/Module.h>
#include <llvm/Target/TargetMachine.h>

// TODO: Current jit implementation is copy paste of llvm::KaleidoscopeJIT
// it is legacy and should be refactored in future

// TODO: Move includes to header

namespace {
using namespace llvm;
using namespace llvm::orc;
} // namespace

namespace pefa::jit {
class JIT {
private:
  std::unique_ptr<EngineBuilder> m_engine_builder;
  ExecutionSession m_session;
  std::map<VModuleKey, std::shared_ptr<SymbolResolver>> m_resolvers;
  TargetMachine *m_target_machine;
  const DataLayout m_data_layout;
  LegacyRTDyldObjectLinkingLayer m_object_layer;
  LegacyIRCompileLayer<decltype(m_object_layer), SimpleCompiler> m_compile_layer;

  using OptimizeFunction = std::function<std::unique_ptr<Module>(std::unique_ptr<Module>)>;

  LegacyIRTransformLayer<decltype(m_compile_layer), OptimizeFunction> m_optimize_layer;

public:
  JIT();

  TargetMachine &getTargetMachine();

  VModuleKey addModule(std::unique_ptr<Module> M);

  JITSymbol findSymbol(const std::string &Name);

  void removeModule(VModuleKey K);

private:
  std::unique_ptr<Module> optimizeModule(std::unique_ptr<Module> M);
};

JIT *get_JIT();
} // namespace pefa::jit

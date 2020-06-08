#pragma once
#include <llvm/ADT/STLExtras.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/Orc/CompileOnDemandLayer.h>
#include <llvm/ExecutionEngine/Orc/CompileUtils.h>
#include <llvm/ExecutionEngine/Orc/IRCompileLayer.h>
#include <llvm/ExecutionEngine/Orc/IRTransformLayer.h>
#include <llvm/ExecutionEngine/Orc/LambdaResolver.h>
#include <llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h>
#include <llvm/ExecutionEngine/RTDyldMemoryManager.h>
#include <llvm/ExecutionEngine/RuntimeDyld.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Mangler.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>

// TODO: Current jit implementation is copy paste of llvm::KaleidoscopeJIT
// it is legacy and should be refactored in future

// TODO: Move includes to header

namespace {
using llvm::AcknowledgeORCv1Deprecation;
using llvm::cantFail;
using llvm::createCFGSimplificationPass;
using llvm::createGVNPass;
using llvm::createInstructionCombiningPass;
using llvm::createReassociatePass;
using llvm::DataLayout;
using llvm::EngineBuilder;
using llvm::Error;
using llvm::Function;
using llvm::JITSymbol;
using llvm::JITSymbolFlags;
using llvm::Mangler;
using llvm::Module;
using llvm::raw_string_ostream;
using llvm::RTDyldMemoryManager;
using llvm::SectionMemoryManager;
using llvm::StringRef;
using llvm::TargetMachine;
using llvm::legacy::FunctionPassManager;
using llvm::orc::createLocalCompileCallbackManager;
using llvm::orc::createLocalIndirectStubsManagerBuilder;
using llvm::orc::ExecutionSession;
using llvm::orc::JITCompileCallbackManager;
using llvm::orc::LegacyCompileOnDemandLayer;
using llvm::orc::LegacyIRCompileLayer;
using llvm::orc::LegacyIRTransformLayer;
using llvm::orc::LegacyRTDyldObjectLinkingLayer;
using llvm::orc::SimpleCompiler;
using llvm::orc::SymbolResolver;
using llvm::orc::VModuleKey;
} // namespace

namespace pefa::jit {
class JIT {
private:
  ExecutionSession m_session;
  std::map<VModuleKey, std::shared_ptr<SymbolResolver>> m_resolvers;
  std::unique_ptr<TargetMachine> m_target_machine;
  const DataLayout m_data_layout;
  LegacyRTDyldObjectLinkingLayer m_object_layer;
  LegacyIRCompileLayer<decltype(m_object_layer), SimpleCompiler> m_compile_layer;

  using OptimizeFunction = std::function<std::unique_ptr<Module>(std::unique_ptr<Module>)>;

  LegacyIRTransformLayer<decltype(m_compile_layer), OptimizeFunction> m_optimize_layer;

  std::unique_ptr<JITCompileCallbackManager> m_compile_callback_manager;
  LegacyCompileOnDemandLayer<decltype(m_optimize_layer)> m_cod_layer;

public:
  JIT();

  TargetMachine &getTargetMachine();

  VModuleKey addModule(std::unique_ptr<Module> M);

  JITSymbol findSymbol(const std::string Name);

  void removeModule(VModuleKey K);

private:
  std::unique_ptr<Module> optimizeModule(std::unique_ptr<Module> M);
};

std::shared_ptr<JIT> get_JIT();
} // namespace pefa::jit

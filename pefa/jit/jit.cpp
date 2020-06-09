#include "jit.h"

#include <llvm/ADT/STLExtras.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/OrcV1Deprecation.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Mangler.h>
#include <llvm/IR/Verifier.h>
#include <llvm/InitializePasses.h>
#include <llvm/PassRegistry.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/AlwaysInliner.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Vectorize.h>

namespace pefa::jit {
JIT::JIT()
    : m_target_machine(EngineBuilder().selectTarget()), m_data_layout(m_target_machine->createDataLayout()),
      m_object_layer(AcknowledgeORCv1Deprecation, m_session,
                     [this](VModuleKey K) {
                       return LegacyRTDyldObjectLinkingLayer::Resources{std::make_shared<SectionMemoryManager>(), m_resolvers[K]};
                     }),
      m_compile_layer(AcknowledgeORCv1Deprecation, m_object_layer, SimpleCompiler(*m_target_machine)),
      m_optimize_layer(AcknowledgeORCv1Deprecation, m_compile_layer,
                       [this](std::unique_ptr<Module> M) { return optimizeModule(std::move(M)); }) {
  llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
}
TargetMachine &JIT::getTargetMachine() {
  return *m_target_machine;
}
VModuleKey JIT::addModule(std::unique_ptr<Module> M) {
  VModuleKey K = m_session.allocateVModule();
  m_resolvers[K] = createLegacyLookupResolver(
      m_session,
      [this](StringRef Name) -> JITSymbol {
        if (auto Sym = m_compile_layer.findSymbol(std::string(Name), false))
          return Sym;
        else if (auto Err = Sym.takeError())
          return std::move(Err);
        if (auto SymAddr = RTDyldMemoryManager::getSymbolAddressInProcess(std::string(Name)))
          return JITSymbol(SymAddr, JITSymbolFlags::Exported);
        return nullptr;
      },
      [](Error Err) { cantFail(std::move(Err), "lookupFlags failed"); });

  cantFail(m_optimize_layer.addModule(K, std::move(M)));
  return K;
}
JITSymbol JIT::findSymbol(std::string Name) {
  std::string MangledName;
  raw_string_ostream MangledNameStream(MangledName);
  Mangler::getNameWithPrefix(MangledNameStream, Name, m_data_layout);
  return m_optimize_layer.findSymbol(MangledNameStream.str(), true);
}
void JIT::removeModule(VModuleKey K) {
  cantFail(m_optimize_layer.removeModule(K));
}
std::unique_ptr<Module> JIT::optimizeModule(std::unique_ptr<Module> M) {
  auto FPM = std::make_unique<llvm::legacy::FunctionPassManager>(M.get());
  auto MPM = std::make_unique<llvm::legacy::PassManager>();
  using namespace llvm;
  PassManagerBuilder Builder;
  Builder.OptLevel = 3;
  Builder.SizeLevel = 0;
  Builder.MergeFunctions = true;
  Builder.Inliner = createFunctionInliningPass(3, 0, false);
  Builder.DisableUnrollLoops = false;
  Builder.LoopVectorize = true;
  Builder.SLPVectorize = true;
  m_target_machine->adjustPassManager(Builder);
  Builder.populateFunctionPassManager(*FPM);
  FPM->doInitialization();
  for (auto &F : *M)
    FPM->run(F);
  FPM->doFinalization();
  MPM->run(*M);
  return M;
}

std::shared_ptr<JIT> get_JIT() {
  static std::shared_ptr<JIT> jit;
  if (jit) {
    return jit;
  } else {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    jit = std::make_shared<JIT>();
    return jit;
  }
}
} // namespace pefa::jit

#include "jit.h"

#include <llvm/ADT/STLExtras.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/ExecutionEngine/OrcV1Deprecation.h>
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Mangler.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

namespace pefa::jit {

std::unique_ptr<EngineBuilder> createEngineBuilder() {
  auto engine_builder = std::make_unique<EngineBuilder>();
  engine_builder->setMCPU(llvm::sys::getHostCPUName());
  engine_builder->setEngineKind(llvm::EngineKind::JIT);
  engine_builder->setOptLevel(llvm::CodeGenOpt::Aggressive);
  llvm::TargetOptions target_options;
  target_options.AllowFPOpFusion = llvm::FPOpFusion::Fast;
  engine_builder->setTargetOptions(target_options);
  return engine_builder;
}

JIT::JIT()
    : m_engine_builder(createEngineBuilder())
    , m_target_machine(m_engine_builder->selectTarget())
    , m_data_layout(m_target_machine->createDataLayout())
    , m_object_layer(AcknowledgeORCv1Deprecation, m_session,
                     [this](VModuleKey K) {
                       return LegacyRTDyldObjectLinkingLayer::Resources{
                           std::make_shared<SectionMemoryManager>(), m_resolvers[K]};
                     })
    , m_compile_layer(AcknowledgeORCv1Deprecation, m_object_layer,
                      SimpleCompiler(*m_target_machine))
    , m_optimize_layer(AcknowledgeORCv1Deprecation, m_compile_layer,
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

void addOptPasses(llvm::legacy::PassManagerBase &passes,
                  llvm::legacy::FunctionPassManager &fnPasses, llvm::TargetMachine &machine) {
  llvm::PassManagerBuilder builder;
  builder.OptLevel = 3;
  builder.SizeLevel = 0;
  builder.Inliner = llvm::createFunctionInliningPass(3, 0, false);
  builder.LoopVectorize = true;
  builder.SLPVectorize = true;
  machine.adjustPassManager(builder);

  builder.populateFunctionPassManager(fnPasses);
  builder.populateModulePassManager(passes);
}

void addLinkPasses(llvm::legacy::PassManagerBase &passes) {
  llvm::PassManagerBuilder builder;
  builder.VerifyInput = true;
  builder.Inliner = llvm::createFunctionInliningPass(3, 0, false);
  builder.populateLTOPassManager(passes);
}

std::unique_ptr<Module> JIT::optimizeModule(std::unique_ptr<Module> module) {
  auto &machine = getTargetMachine();
  llvm::legacy::PassManager passes;
  passes.add(llvm::createVerifierPass());
  passes.add(new llvm::TargetLibraryInfoWrapperPass(machine.getTargetTriple()));
  passes.add(llvm::createTargetTransformInfoWrapperPass(machine.getTargetIRAnalysis()));

  llvm::legacy::FunctionPassManager fnPasses(module.get());
  fnPasses.add(llvm::createTargetTransformInfoWrapperPass(machine.getTargetIRAnalysis()));

  addOptPasses(passes, fnPasses, machine);
  addLinkPasses(passes);

  fnPasses.doInitialization();
  for (llvm::Function &func : *module) {
    fnPasses.run(func);
  }
  fnPasses.doFinalization();

  passes.add(llvm::createVerifierPass());
  passes.run(*module);
  return module;
}
std::shared_ptr<JIT> get_JIT() {
  static std::shared_ptr<JIT> jit;
  if (jit) {
    return jit;
  } else {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmParser();
    llvm::InitializeNativeTargetAsmPrinter();
    jit = std::make_shared<JIT>();
    return jit;
  }
}
} // namespace pefa::jit

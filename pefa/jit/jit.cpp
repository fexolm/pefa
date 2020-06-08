//
// Created by fexolm on 06.06.2020.
//

#include "jit.h"
#include "llvm/Transforms/Vectorize.h"
#include "llvm/Transforms/IPO/AlwaysInliner.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/PassRegistry.h"
#include "llvm/InitializePasses.h"

namespace pefa::jit {
JIT::JIT()
    : m_target_machine(EngineBuilder().selectTarget()),
      m_data_layout(m_target_machine->createDataLayout()),
      m_object_layer(AcknowledgeORCv1Deprecation, m_session,
                     [this](VModuleKey K) {
                       return LegacyRTDyldObjectLinkingLayer::Resources{
                           std::make_shared<SectionMemoryManager>(), m_resolvers[K]};
                     }),
      m_compile_layer(AcknowledgeORCv1Deprecation, m_object_layer,
                      SimpleCompiler(*m_target_machine)),
      m_optimize_layer(AcknowledgeORCv1Deprecation, m_compile_layer,
                       [this](std::unique_ptr<Module> M) { return optimizeModule(std::move(M)); }),
      m_compile_callback_manager(cantFail(
          createLocalCompileCallbackManager(m_target_machine->getTargetTriple(), m_session, 0))),
      m_cod_layer(
          AcknowledgeORCv1Deprecation, m_session, m_optimize_layer,
          [&](VModuleKey K) { return m_resolvers[K]; },
          [&](VModuleKey K, std::shared_ptr<SymbolResolver> R) { m_resolvers[K] = std::move(R); },
          [](Function &F) { return std::set<Function *>({&F}); }, *m_compile_callback_manager,
          createLocalIndirectStubsManagerBuilder(m_target_machine->getTargetTriple())) {
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

  cantFail(m_cod_layer.addModule(K, std::move(M)));
  return K;
}
JITSymbol JIT::findSymbol(std::string Name) {
  std::string MangledName;
  raw_string_ostream MangledNameStream(MangledName);
  Mangler::getNameWithPrefix(MangledNameStream, Name, m_data_layout);
  return m_cod_layer.findSymbol(MangledNameStream.str(), true);
}
void JIT::removeModule(VModuleKey K) {
  cantFail(m_cod_layer.removeModule(K));
}
std::unique_ptr<Module> JIT::optimizeModule(std::unique_ptr<Module> M) {
  auto FPM = std::make_unique<FunctionPassManager>(M.get());
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
  M->print(llvm::errs(), nullptr);
  MPM->run(*M);
  for (auto &F : *M)
    FPM->run(F);
  MPM->run(*M);
  for (auto &F : *M)
    FPM->run(F);

  M->print(llvm::errs(), nullptr);
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
    using namespace llvm;
    // Initialize passes
    PassRegistry &Registry = *PassRegistry::getPassRegistry();
    initializeCore(Registry);
    initializeCoroutines(Registry);
    initializeScalarOpts(Registry);
    initializeObjCARCOpts(Registry);
    initializeVectorization(Registry);
    initializeIPO(Registry);
    initializeAnalysis(Registry);
    initializeTransformUtils(Registry);
    initializeInstCombine(Registry);
    initializeAggressiveInstCombine(Registry);
    initializeInstrumentation(Registry);
    initializeTarget(Registry);
    // For codegen passes, only passes that do IR to IR transformation are
    // supported.
    initializeExpandMemCmpPassPass(Registry);
    initializeScalarizeMaskedMemIntrinPass(Registry);
    initializeCodeGenPreparePass(Registry);
    initializeAtomicExpandPass(Registry);
    initializeRewriteSymbolsLegacyPassPass(Registry);
    initializeWinEHPreparePass(Registry);
    initializeDwarfEHPreparePass(Registry);
    initializeSafeStackLegacyPassPass(Registry);
    initializeSjLjEHPreparePass(Registry);
    initializePreISelIntrinsicLoweringLegacyPassPass(Registry);
    initializeGlobalMergePass(Registry);
    initializeIndirectBrExpandPassPass(Registry);
    initializeInterleavedLoadCombinePass(Registry);
    initializeInterleavedAccessPass(Registry);
    initializeEntryExitInstrumenterPass(Registry);
    initializePostInlineEntryExitInstrumenterPass(Registry);
    initializeUnreachableBlockElimLegacyPassPass(Registry);
    initializeExpandReductionsPass(Registry);
    initializeWasmEHPreparePass(Registry);
    initializeWriteBitcodePassPass(Registry);
    initializeHardwareLoopsPass(Registry);
    jit = std::make_shared<JIT>();
    return jit;
  }
}
} // namespace pefa::jit

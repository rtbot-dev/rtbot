#include "rtbot/compiled/jit/JitContext.h"

#include <llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/Passes/OptimizationLevel.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>

namespace rtbot {

JitContext::JitContext() {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  auto jtmb = llvm::orc::JITTargetMachineBuilder::detectHost();
  if (!jtmb) {
    std::string msg = llvm::toString(jtmb.takeError());
    llvm::report_fatal_error(llvm::StringRef(msg));
  }
  jtmb->setCodeGenOptLevel(llvm::CodeGenOpt::Aggressive);

  auto jit_or_err = llvm::orc::LLJITBuilder()
                        .setJITTargetMachineBuilder(std::move(*jtmb))
                        .create();
  if (!jit_or_err) {
    std::string msg = llvm::toString(jit_or_err.takeError());
    llvm::report_fatal_error(llvm::StringRef(msg));
  }
  jit_ = std::move(*jit_or_err);
}

JitContext::~JitContext() = default;

void JitContext::compile_module(std::unique_ptr<llvm::Module> module,
                                std::unique_ptr<llvm::LLVMContext> ctx) {
  // Run the standard O3 IR optimization pipeline before codegen.
  // This promotes allocas, runs instcombine, loop-unroll, etc. — all passes
  // that the backend CodeGenOpt level does NOT cover.
  {
    llvm::PassBuilder pb;
    llvm::LoopAnalysisManager lam;
    llvm::FunctionAnalysisManager fam;
    llvm::CGSCCAnalysisManager cgam;
    llvm::ModuleAnalysisManager mam;
    pb.registerModuleAnalyses(mam);
    pb.registerCGSCCAnalyses(cgam);
    pb.registerFunctionAnalyses(fam);
    pb.registerLoopAnalyses(lam);
    pb.crossRegisterProxies(lam, fam, cgam, mam);
    llvm::ModulePassManager mpm =
        pb.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O3);
    mpm.run(*module, mam);
  }

  llvm::orc::ThreadSafeModule tsm(std::move(module), std::move(ctx));
  if (auto err = jit_->addIRModule(std::move(tsm))) {
    std::string msg = llvm::toString(std::move(err));
    llvm::report_fatal_error(llvm::StringRef(msg));
  }
}

}  // namespace rtbot

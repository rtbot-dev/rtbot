#pragma once

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Error.h>

#include <memory>
#include <string>

namespace rtbot {

class JitContext {
 public:
  JitContext();
  ~JitContext();

  // Compile a module. Takes ownership of the module + its LLVMContext via
  // ThreadSafeModule. Calls llvm::report_fatal_error on failure.
  void compile_module(std::unique_ptr<llvm::Module> module,
                      std::unique_ptr<llvm::LLVMContext> ctx);

  // Look up a JIT'd function by name. Returns nullptr if not found.
  template <class FnPtrT>
  FnPtrT lookup(const std::string& name) {
    auto sym = jit_->lookup(name);
    if (!sym) {
      llvm::consumeError(sym.takeError());
      return nullptr;
    }
    return sym->toPtr<FnPtrT>();
  }

 private:
  std::unique_ptr<llvm::orc::LLJIT> jit_;
};

}  // namespace rtbot

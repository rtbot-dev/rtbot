#include <catch2/catch.hpp>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

#include <memory>

#include "rtbot/compiled/jit/IrEmissionContext.h"
#include "rtbot/compiled/jit/emit/Boolean.h"
#include "libs/compiled/jit_spike/JitContext.h"

using namespace rtbot::jit;
using namespace rtbot::jit::emit;

// ---------------------------------------------------------------------------
// AND
// ---------------------------------------------------------------------------
SCENARIO("emit_and JIT'd to native and", "[boolean][and]") {
  static rtbot::JitContext jit;

  auto ctx = std::make_unique<llvm::LLVMContext>();
  auto mod = std::make_unique<llvm::Module>("test_and", *ctx);
  llvm::Type* dbl = llvm::Type::getDoubleTy(*ctx);
  llvm::FunctionType* ft = llvm::FunctionType::get(dbl, {dbl, dbl}, false);
  llvm::Function* fn =
      llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_and", mod.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*ctx, "entry", fn);
  llvm::IRBuilder<> b(bb);
  IrEmissionContext ec(*ctx, *mod, b, /*state_ptr=*/nullptr);
  llvm::Value* result = emit_and(ec, fn->getArg(0), fn->getArg(1));
  b.CreateRet(result);

  jit.compile_module(std::move(mod), std::move(ctx));
  auto and_fn = jit.lookup<double (*)(double, double)>("test_and");

  REQUIRE(and_fn != nullptr);
  REQUIRE(and_fn(0.0, 0.0) == 0.0);   // false AND false => false
  REQUIRE(and_fn(1.0, 0.0) == 0.0);   // true  AND false => false
  REQUIRE(and_fn(0.0, 1.0) == 0.0);   // false AND true  => false
  REQUIRE(and_fn(1.0, 1.0) == 1.0);   // true  AND true  => true
  REQUIRE(and_fn(42.0, -3.5) == 1.0); // both non-zero   => true
  REQUIRE(and_fn(0.0, -3.5) == 0.0);  // first zero      => false
}

// ---------------------------------------------------------------------------
// OR
// ---------------------------------------------------------------------------
SCENARIO("emit_or JIT'd to native or", "[boolean][or]") {
  static rtbot::JitContext jit;

  auto ctx = std::make_unique<llvm::LLVMContext>();
  auto mod = std::make_unique<llvm::Module>("test_or", *ctx);
  llvm::Type* dbl = llvm::Type::getDoubleTy(*ctx);
  llvm::FunctionType* ft = llvm::FunctionType::get(dbl, {dbl, dbl}, false);
  llvm::Function* fn =
      llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_or", mod.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*ctx, "entry", fn);
  llvm::IRBuilder<> b(bb);
  IrEmissionContext ec(*ctx, *mod, b, /*state_ptr=*/nullptr);
  llvm::Value* result = emit_or(ec, fn->getArg(0), fn->getArg(1));
  b.CreateRet(result);

  jit.compile_module(std::move(mod), std::move(ctx));
  auto or_fn = jit.lookup<double (*)(double, double)>("test_or");

  REQUIRE(or_fn != nullptr);
  REQUIRE(or_fn(0.0, 0.0) == 0.0);   // false OR false => false
  REQUIRE(or_fn(0.0, 1.0) == 1.0);   // false OR true  => true
  REQUIRE(or_fn(1.0, 0.0) == 1.0);   // true  OR false => true
  REQUIRE(or_fn(1.0, 1.0) == 1.0);   // true  OR true  => true
  REQUIRE(or_fn(42.0, -3.5) == 1.0); // both non-zero  => true
  REQUIRE(or_fn(0.0, -3.5) == 1.0);  // second non-zero => true
}

// ---------------------------------------------------------------------------
// NOT
// ---------------------------------------------------------------------------
SCENARIO("emit_not JIT'd to native not", "[boolean][not]") {
  static rtbot::JitContext jit;

  auto ctx = std::make_unique<llvm::LLVMContext>();
  auto mod = std::make_unique<llvm::Module>("test_not", *ctx);
  llvm::Type* dbl = llvm::Type::getDoubleTy(*ctx);
  llvm::FunctionType* ft = llvm::FunctionType::get(dbl, {dbl}, false);
  llvm::Function* fn =
      llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_not", mod.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*ctx, "entry", fn);
  llvm::IRBuilder<> b(bb);
  IrEmissionContext ec(*ctx, *mod, b, /*state_ptr=*/nullptr);
  llvm::Value* result = emit_not(ec, fn->getArg(0));
  b.CreateRet(result);

  jit.compile_module(std::move(mod), std::move(ctx));
  auto not_fn = jit.lookup<double (*)(double)>("test_not");

  REQUIRE(not_fn != nullptr);
  REQUIRE(not_fn(0.0) == 1.0);   // NOT false => true
  REQUIRE(not_fn(1.0) == 0.0);   // NOT true  => false
  REQUIRE(not_fn(42.0) == 0.0);  // NOT non-zero => false
  REQUIRE(not_fn(-1.5) == 0.0);  // NOT negative non-zero => false
  REQUIRE(not_fn(-0.0) == 1.0);  // NOT -0.0 => true (IEEE-754: -0.0 == 0.0)
}

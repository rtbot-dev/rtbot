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
#include "rtbot/compiled/jit/emit/Arithmetic.h"
#include "libs/compiled/jit_spike/JitContext.h"

using namespace rtbot::jit;
using namespace rtbot::jit::emit;

SCENARIO("emit_add JIT'd to native add", "[arithmetic][add]") {
  static rtbot::JitContext jit;

  auto ctx = std::make_unique<llvm::LLVMContext>();
  auto mod = std::make_unique<llvm::Module>("test_add", *ctx);
  llvm::Type* dbl = llvm::Type::getDoubleTy(*ctx);
  llvm::FunctionType* ft = llvm::FunctionType::get(dbl, {dbl, dbl}, false);
  llvm::Function* fn =
      llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_add", mod.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*ctx, "entry", fn);
  llvm::IRBuilder<> b(bb);
  IrEmissionContext ec(*ctx, *mod, b, /*state_ptr=*/nullptr);
  llvm::Value* result = emit_add(ec, fn->getArg(0), fn->getArg(1));
  b.CreateRet(result);

  jit.compile_module(std::move(mod), std::move(ctx));
  auto add = jit.lookup<double (*)(double, double)>("test_add");

  REQUIRE(add != nullptr);
  REQUIRE(add(1.5, 2.25) == 3.75);
  REQUIRE(add(-1.0, 1.0) == 0.0);
  REQUIRE(add(0.0, 0.0) == 0.0);
}

SCENARIO("emit_sub JIT'd to native sub", "[arithmetic][sub]") {
  static rtbot::JitContext jit;

  auto ctx = std::make_unique<llvm::LLVMContext>();
  auto mod = std::make_unique<llvm::Module>("test_sub", *ctx);
  llvm::Type* dbl = llvm::Type::getDoubleTy(*ctx);
  llvm::FunctionType* ft = llvm::FunctionType::get(dbl, {dbl, dbl}, false);
  llvm::Function* fn =
      llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_sub", mod.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*ctx, "entry", fn);
  llvm::IRBuilder<> b(bb);
  IrEmissionContext ec(*ctx, *mod, b, /*state_ptr=*/nullptr);
  llvm::Value* result = emit_sub(ec, fn->getArg(0), fn->getArg(1));
  b.CreateRet(result);

  jit.compile_module(std::move(mod), std::move(ctx));
  auto sub = jit.lookup<double (*)(double, double)>("test_sub");

  REQUIRE(sub != nullptr);
  REQUIRE(sub(5.0, 2.0) == 3.0);
  REQUIRE(sub(1.0, 1.0) == 0.0);
  REQUIRE(sub(0.0, 0.5) == -0.5);
}

SCENARIO("emit_mul JIT'd to native mul", "[arithmetic][mul]") {
  static rtbot::JitContext jit;

  auto ctx = std::make_unique<llvm::LLVMContext>();
  auto mod = std::make_unique<llvm::Module>("test_mul", *ctx);
  llvm::Type* dbl = llvm::Type::getDoubleTy(*ctx);
  llvm::FunctionType* ft = llvm::FunctionType::get(dbl, {dbl, dbl}, false);
  llvm::Function* fn =
      llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_mul", mod.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*ctx, "entry", fn);
  llvm::IRBuilder<> b(bb);
  IrEmissionContext ec(*ctx, *mod, b, /*state_ptr=*/nullptr);
  llvm::Value* result = emit_mul(ec, fn->getArg(0), fn->getArg(1));
  b.CreateRet(result);

  jit.compile_module(std::move(mod), std::move(ctx));
  auto mul = jit.lookup<double (*)(double, double)>("test_mul");

  REQUIRE(mul != nullptr);
  REQUIRE(mul(2.0, 3.0) == 6.0);
  REQUIRE(mul(-1.0, 4.0) == -4.0);
  REQUIRE(mul(0.0, 999.0) == 0.0);
}

SCENARIO("emit_div JIT'd to native div", "[arithmetic][div]") {
  static rtbot::JitContext jit;

  auto ctx = std::make_unique<llvm::LLVMContext>();
  auto mod = std::make_unique<llvm::Module>("test_div", *ctx);
  llvm::Type* dbl = llvm::Type::getDoubleTy(*ctx);
  llvm::FunctionType* ft = llvm::FunctionType::get(dbl, {dbl, dbl}, false);
  llvm::Function* fn =
      llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_div", mod.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*ctx, "entry", fn);
  llvm::IRBuilder<> b(bb);
  IrEmissionContext ec(*ctx, *mod, b, /*state_ptr=*/nullptr);
  llvm::Value* result = emit_div(ec, fn->getArg(0), fn->getArg(1));
  b.CreateRet(result);

  jit.compile_module(std::move(mod), std::move(ctx));
  auto div_fn = jit.lookup<double (*)(double, double)>("test_div");

  REQUIRE(div_fn != nullptr);
  REQUIRE(div_fn(6.0, 2.0) == 3.0);
  REQUIRE(div_fn(1.0, 4.0) == 0.25);
  REQUIRE(div_fn(-9.0, 3.0) == -3.0);
}

SCENARIO("emit_scale JIT'd to native scale", "[arithmetic][scale]") {
  static rtbot::JitContext jit;

  auto ctx = std::make_unique<llvm::LLVMContext>();
  auto mod = std::make_unique<llvm::Module>("test_scale", *ctx);
  llvm::Type* dbl = llvm::Type::getDoubleTy(*ctx);
  llvm::FunctionType* ft = llvm::FunctionType::get(dbl, {dbl}, false);
  llvm::Function* fn =
      llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_scale", mod.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*ctx, "entry", fn);
  llvm::IRBuilder<> b(bb);
  IrEmissionContext ec(*ctx, *mod, b, /*state_ptr=*/nullptr);
  llvm::Value* result = emit_scale(ec, fn->getArg(0), 2.0);
  b.CreateRet(result);

  jit.compile_module(std::move(mod), std::move(ctx));
  auto scale = jit.lookup<double (*)(double)>("test_scale");

  REQUIRE(scale != nullptr);
  REQUIRE(scale(3.5) == 7.0);
  REQUIRE(scale(-1.0) == -2.0);
  REQUIRE(scale(0.0) == 0.0);
}

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
#include "rtbot/compiled/jit/emit/Comparison.h"
#include "libs/compiled/jit_spike/JitContext.h"

using namespace rtbot::jit;
using namespace rtbot::jit::emit;

// ---------------------------------------------------------------------------
// Helper: build a double(double, double) function, emit the comparison inside
// it, JIT-compile it, and return the callable.
// ---------------------------------------------------------------------------
static double (*build_cmp_fn(
    rtbot::JitContext& jit,
    const char* name,
    llvm::Value* (*emitter)(IrEmissionContext&, llvm::Value*, llvm::Value*)))(double, double) {
  auto ctx = std::make_unique<llvm::LLVMContext>();
  auto mod = std::make_unique<llvm::Module>(name, *ctx);
  llvm::Type* dbl = llvm::Type::getDoubleTy(*ctx);
  llvm::FunctionType* ft = llvm::FunctionType::get(dbl, {dbl, dbl}, false);
  llvm::Function* fn =
      llvm::Function::Create(ft, llvm::Function::ExternalLinkage, name, mod.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*ctx, "entry", fn);
  llvm::IRBuilder<> b(bb);
  IrEmissionContext ec(*ctx, *mod, b, /*state_ptr=*/nullptr);
  llvm::Value* result = emitter(ec, fn->getArg(0), fn->getArg(1));
  b.CreateRet(result);

  jit.compile_module(std::move(mod), std::move(ctx));
  return jit.lookup<double (*)(double, double)>(name);
}

// ---------------------------------------------------------------------------
// GT
// ---------------------------------------------------------------------------
SCENARIO("emit_gt JIT'd to native gt", "[comparison][gt]") {
  static rtbot::JitContext jit;

  auto gt = build_cmp_fn(jit, "test_gt", emit_gt);
  REQUIRE(gt != nullptr);

  // equal: 1.0 > 1.0  => false
  REQUIRE(gt(1.0, 1.0) == 0.0);
  // strictly less: 1.0 > 2.0 => false
  REQUIRE(gt(1.0, 2.0) == 0.0);
  // strictly greater: 2.0 > 1.0 => true
  REQUIRE(gt(2.0, 1.0) == 1.0);
  // negative: -1.0 > 1.0 => false
  REQUIRE(gt(-1.0, 1.0) == 0.0);
  // zero: 0.0 > 0.0 => false
  REQUIRE(gt(0.0, 0.0) == 0.0);
}

// ---------------------------------------------------------------------------
// GTE
// ---------------------------------------------------------------------------
SCENARIO("emit_gte JIT'd to native gte", "[comparison][gte]") {
  static rtbot::JitContext jit;

  auto gte = build_cmp_fn(jit, "test_gte", emit_gte);
  REQUIRE(gte != nullptr);

  // equal: 1.0 >= 1.0 => true
  REQUIRE(gte(1.0, 1.0) == 1.0);
  // strictly less: 1.0 >= 2.0 => false
  REQUIRE(gte(1.0, 2.0) == 0.0);
  // strictly greater: 2.0 >= 1.0 => true
  REQUIRE(gte(2.0, 1.0) == 1.0);
  // negative: -1.0 >= 1.0 => false
  REQUIRE(gte(-1.0, 1.0) == 0.0);
  // zero: 0.0 >= 0.0 => true
  REQUIRE(gte(0.0, 0.0) == 1.0);
}

// ---------------------------------------------------------------------------
// LT
// ---------------------------------------------------------------------------
SCENARIO("emit_lt JIT'd to native lt", "[comparison][lt]") {
  static rtbot::JitContext jit;

  auto lt = build_cmp_fn(jit, "test_lt", emit_lt);
  REQUIRE(lt != nullptr);

  // equal: 1.0 < 1.0 => false
  REQUIRE(lt(1.0, 1.0) == 0.0);
  // strictly less: 1.0 < 2.0 => true
  REQUIRE(lt(1.0, 2.0) == 1.0);
  // strictly greater: 2.0 < 1.0 => false
  REQUIRE(lt(2.0, 1.0) == 0.0);
  // negative: -1.0 < 1.0 => true
  REQUIRE(lt(-1.0, 1.0) == 1.0);
  // zero: 0.0 < 0.0 => false
  REQUIRE(lt(0.0, 0.0) == 0.0);
}

// ---------------------------------------------------------------------------
// LTE
// ---------------------------------------------------------------------------
SCENARIO("emit_lte JIT'd to native lte", "[comparison][lte]") {
  static rtbot::JitContext jit;

  auto lte = build_cmp_fn(jit, "test_lte", emit_lte);
  REQUIRE(lte != nullptr);

  // equal: 1.0 <= 1.0 => true
  REQUIRE(lte(1.0, 1.0) == 1.0);
  // strictly less: 1.0 <= 2.0 => true
  REQUIRE(lte(1.0, 2.0) == 1.0);
  // strictly greater: 2.0 <= 1.0 => false
  REQUIRE(lte(2.0, 1.0) == 0.0);
  // negative: -1.0 <= 1.0 => true
  REQUIRE(lte(-1.0, 1.0) == 1.0);
  // zero: 0.0 <= 0.0 => true
  REQUIRE(lte(0.0, 0.0) == 1.0);
}

// ---------------------------------------------------------------------------
// EQ
// ---------------------------------------------------------------------------
SCENARIO("emit_eq JIT'd to native eq", "[comparison][eq]") {
  static rtbot::JitContext jit;

  auto eq = build_cmp_fn(jit, "test_eq", emit_eq);
  REQUIRE(eq != nullptr);

  // equal: 1.0 == 1.0 => true
  REQUIRE(eq(1.0, 1.0) == 1.0);
  // strictly less: 1.0 == 2.0 => false
  REQUIRE(eq(1.0, 2.0) == 0.0);
  // strictly greater: 2.0 == 1.0 => false
  REQUIRE(eq(2.0, 1.0) == 0.0);
  // negative: -1.0 == 1.0 => false
  REQUIRE(eq(-1.0, 1.0) == 0.0);
  // zero: 0.0 == 0.0 => true
  REQUIRE(eq(0.0, 0.0) == 1.0);
}

// ---------------------------------------------------------------------------
// NEQ
// ---------------------------------------------------------------------------
SCENARIO("emit_neq JIT'd to native neq", "[comparison][neq]") {
  static rtbot::JitContext jit;

  auto neq = build_cmp_fn(jit, "test_neq", emit_neq);
  REQUIRE(neq != nullptr);

  // equal: 1.0 != 1.0 => false
  REQUIRE(neq(1.0, 1.0) == 0.0);
  // strictly less: 1.0 != 2.0 => true
  REQUIRE(neq(1.0, 2.0) == 1.0);
  // strictly greater: 2.0 != 1.0 => true
  REQUIRE(neq(2.0, 1.0) == 1.0);
  // negative: -1.0 != 1.0 => true
  REQUIRE(neq(-1.0, 1.0) == 1.0);
  // zero: 0.0 != 0.0 => false
  REQUIRE(neq(0.0, 0.0) == 0.0);
}

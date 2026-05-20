#include "rtbot/compiled/jit/emit/TimeShift.h"

#include <llvm/IR/Constants.h>
#include <llvm/IR/Type.h>

namespace rtbot::jit::emit {

StatefulOutput emit_time_shift(IrEmissionContext& ec, std::int64_t shift,
                                llvm::Value* t, llvm::Value* v) {
  auto& b   = ec.b();
  auto& ctx = ec.ctx();

  llvm::Type* i64 = llvm::Type::getInt64Ty(ctx);

  llvm::Value* shift_val = llvm::ConstantInt::get(i64, shift);
  llvm::Value* new_t     = b.CreateAdd(t, shift_val, "ts_new_t");
  llvm::Value* zero      = llvm::ConstantInt::get(i64, 0);
  llvm::Value* keep      = b.CreateICmpSGE(new_t, zero, "ts_keep");

  return StatefulOutput{new_t, v, keep};
}

}  // namespace rtbot::jit::emit

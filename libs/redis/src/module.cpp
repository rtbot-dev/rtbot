#include "module.h"

#include <stdlib.h>

int RtBotRand_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  RedisModule_ReplyWithLongLong(ctx, rand());
  return REDISMODULE_OK;
}

// we need the `extern "C"` here to prevent symbol name mangling
// it is likely that there is another way of achieving the same
// with certain compiler options
extern "C" {
int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  if (RedisModule_Init(ctx, "rtbot", 1, REDISMODULE_APIVER_1) == REDISMODULE_ERR) return REDISMODULE_ERR;

  if (RedisModule_CreateCommand(ctx, "rtbot.rand", RtBotRand_RedisCommand, "fast random", 0, 0, 0) == REDISMODULE_ERR)
    return REDISMODULE_ERR;

  return REDISMODULE_OK;
}
}

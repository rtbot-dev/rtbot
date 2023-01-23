#ifndef MODULE_H
#define MODULE_H
#include "redismodule.h"

int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc);

#endif

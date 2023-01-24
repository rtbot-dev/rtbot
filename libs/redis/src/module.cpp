#include "module.h"

#include <stdlib.h>

#include <iostream>

int RtBotRun_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  // TODO: it should be 4, once we specify the prefix
  if (argc != 3) {
    return RedisModule_WrongArity(ctx);
  }
  /*char msg2[120];
  size_t len2;
  sprintf(msg2, "Args passed %s, %s, %s", RedisModule_StringPtrLen(argv[0], &len2), RedisModule_StringPtrLen(argv[1],
  &len2), RedisModule_StringPtrLen(argv[2], &len2) );

  RedisModule_Log(ctx, "warning", msg2);*/

  // program key
  RedisModuleKey *pKey = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  if (RedisModule_KeyType(pKey) != REDISMODULE_KEYTYPE_STRING) {
    RedisModule_CloseKey(pKey);
    char msg[120];
    size_t len;
    sprintf(msg, "Object at key=%s has a wrong type %i,  expected 'string'", RedisModule_StringPtrLen(argv[1], &len),
            RedisModule_KeyType(pKey));
    return RedisModule_ReplyWithError(ctx, msg);
  }
  RedisModule_CloseKey(pKey);

  // input key
  RedisModuleKey *tsKey = RedisModule_OpenKey(ctx, argv[2], REDISMODULE_READ);
  if (RedisModule_KeyType(tsKey) != REDISMODULE_KEYTYPE_MODULE) {
    RedisModule_CloseKey(tsKey);
    char msg[120];
    size_t len;
    sprintf(msg, "Object at key=%s has type %i, expected %i", RedisModule_StringPtrLen(argv[2], &len),
            RedisModule_KeyType(tsKey), REDISMODULE_KEYTYPE_MODULE);
    return RedisModule_ReplyWithError(ctx, msg);
  }
  RedisModule_CloseKey(tsKey);

  // get the values of the timeseries at the input key
  RedisModuleCallReply *reply;
  reply = RedisModule_Call(ctx, "ts.range", "scccc", argv[2], "-", "+", "count", "5");
  if (RedisModule_CallReplyType(reply) != REDISMODULE_REPLY_ARRAY) {
    char msg[120];
    sprintf(msg, "ts.range returned a wrong reply type=%i, expected %i (REDISMODULE_REPLY_ARRAY)",
            RedisModule_CallReplyType(reply), REDISMODULE_REPLY_ARRAY);
    return RedisModule_ReplyWithError(ctx, msg);
  }

  size_t reply_len = RedisModule_CallReplyLength(reply);

  for (size_t i = 0; i < reply_len; i++) {
    RedisModuleCallReply *row = RedisModule_CallReplyArrayElement(reply, i);

    RedisModuleCallReply *ts = RedisModule_CallReplyArrayElement(row, 0);
    long long timestamp = RedisModule_CallReplyInteger(ts);
    RedisModuleCallReply *val = RedisModule_CallReplyArrayElement(row, 1);
    RedisModuleString *str = RedisModule_CreateStringFromCallReply(val);

    char msg2[120];
    size_t len2;
    sprintf(msg2, "(%llo, %s)", timestamp, RedisModule_StringPtrLen(str, &len2));

    RedisModule_Log(ctx, "warning", msg2);
    RedisModule_ReplyWithString(ctx, str);
  }

  // double y = RedisModule_CreateStringFromCallReply(val);

  // here is where we iterate and send all the content of the timeseries
  // to our rtbot program

  // RedisModule_ReplyWithLongLong(ctx, timestamp);
  // RedisModule_ReplyWithDouble(ctx, y);
  return REDISMODULE_OK;
}

// we need the `extern "C"` here to prevent symbol name mangling
// it is likely that there is another way of achieving the same
// with certain compiler options
extern "C" {
int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  if (RedisModule_Init(ctx, "rtbot", 1, REDISMODULE_APIVER_1) == REDISMODULE_ERR) return REDISMODULE_ERR;

  if (RedisModule_CreateCommand(ctx, "rtbot.run", RtBotRun_RedisCommand, "write", 0, 0, 0) == REDISMODULE_ERR)
    return REDISMODULE_ERR;

  return REDISMODULE_OK;
}
}

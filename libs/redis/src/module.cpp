#include "module.h"
#include "rtbot/FactoryOp.h"
#include "fast_double_parser.h"
#include <stdlib.h>
#include <optional>
#include <iostream>
#include <string.h>
#include <nlohmann/json.hpp>


int RtBotRun_RedisCommand(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
  std::cout << "Running rtbot.run command" << std::endl;
  if (argc != 4) {
    return RedisModule_WrongArity(ctx);
  }
  /*char msg2[120];
  size_t len2;
  sprintf(msg2, "Args passed %s, %s, %s", RedisModule_StringPtrLen(argv[0], &len2), RedisModule_StringPtrLen(argv[1],
  &len2), RedisModule_StringPtrLen(argv[2], &len2) );

  RedisModule_Log(ctx, "warning", msg2);*/

  // program key, notice that we store it as a native json in redis
  RedisModuleKey *pKey = RedisModule_OpenKey(ctx, argv[1], REDISMODULE_READ);
  if (RedisModule_KeyType(pKey) != REDISMODULE_KEYTYPE_MODULE) {
    RedisModule_CloseKey(pKey);
    char msg[120];
    sprintf(msg, "Object at key=%s has a wrong type %i, expected 'json'", RedisModule_StringPtrLen(argv[1], NULL),
            RedisModule_KeyType(pKey));
    return RedisModule_ReplyWithError(ctx, msg);
  }
  RedisModule_CloseKey(pKey);
  // read the program
  RedisModuleCallReply *reply;
  reply = RedisModule_Call(ctx, "json.get", "s", argv[1]);
  if (RedisModule_CallReplyType(reply) != REDISMODULE_REPLY_STRING) {
    char msg[120];
    sprintf(msg, "ts.range returned a wrong reply type=%i, expected %i (REDISMODULE_REPLY_STRING)",
            RedisModule_CallReplyType(reply), REDISMODULE_REPLY_STRING);
    return RedisModule_ReplyWithError(ctx, msg);
  }
  RedisModuleString *programStr = RedisModule_CreateStringFromCallReply(reply);
  const char *program = RedisModule_StringPtrLen(programStr, NULL);
  RedisModule_Log(ctx, "warning", program);
  // TODO: for some reason we need to upgrade the standard library so object
  // inside the redis-stack docker container with the command:
  // apt-get upgrade libstdc++6
  // otherwise the module won't be loaded if we use the next line of code
  nlohmann::json programJson = nlohmann::json::parse(program);
  auto pipeline = rtbot::FactoryOp::createPipeline(programJson);

  // input key
  RedisModuleKey *tsKey = RedisModule_OpenKey(ctx, argv[2], REDISMODULE_READ);
  if (RedisModule_KeyType(tsKey) != REDISMODULE_KEYTYPE_MODULE) {
    RedisModule_CloseKey(tsKey);
    char msg[120];
    sprintf(msg, "Object at key=%s has type %i, expected %i", RedisModule_StringPtrLen(argv[2], NULL),
            RedisModule_KeyType(tsKey), REDISMODULE_KEYTYPE_MODULE);
    return RedisModule_ReplyWithError(ctx, msg);
  }
  RedisModule_CloseKey(tsKey);

  // get the values of the timeseries at the input key
  std::cout << "Executing ts.range " << argv[2] << "- +" << std::endl;
  reply = RedisModule_Call(ctx, "ts.range", "scc", argv[2], "-", "+");
  if (RedisModule_CallReplyType(reply) != REDISMODULE_REPLY_ARRAY) {
    char msg[120];
    sprintf(msg, "ts.range returned a wrong reply type=%i, expected %i (REDISMODULE_REPLY_ARRAY)",
            RedisModule_CallReplyType(reply), REDISMODULE_REPLY_ARRAY);
    return RedisModule_ReplyWithError(ctx, msg);
  }

  size_t reply_len = RedisModule_CallReplyLength(reply);
  RedisModule_Log(ctx, "warning", "Sample size %lu", reply_len);

  std::cout<<"Sample size" << reply_len << std::endl <<std::flush;
  for (size_t i = 0; i < reply_len; i++) {
    RedisModuleCallReply *row = RedisModule_CallReplyArrayElement(reply, i);

    RedisModuleCallReply *ts = RedisModule_CallReplyArrayElement(row, 0);
    long long timestamp = RedisModule_CallReplyInteger(ts);

    RedisModuleCallReply *val = RedisModule_CallReplyArrayElement(row, 1);
    RedisModuleString *valueStr = RedisModule_CreateStringFromCallReply(val);
    const char *valueCStr = RedisModule_StringPtrLen(valueStr, NULL);
    double value;
    if ((fast_double_parser::parse_number(valueCStr, &value) == NULL)) {
      return RedisModule_ReplyWithError(ctx, "Value in timeseries is not a valid double");
    }

    std::optional<rtbot::Message<double>> result = pipeline.receive(rtbot::Message<>((int)timestamp, value));
    if(result.has_value()) {
      // RedisModule_Log(ctx, "warning", "New result received (%i, %f)", result->time, result->value.at(0));
      RedisModuleCallReply *addReply = RedisModule_Call(ctx, "ts.add", "slc", argv[3], (long long)result->time, std::to_string(result->value.at(0)).c_str());
      if (RedisModule_CallReplyType(addReply) != REDISMODULE_REPLY_INTEGER) {
        RedisModule_Log(ctx, "warning", "ts.add returned a wrong reply type=%i, expected %i (REDISMODULE_REPLY_INTEGER)",
                                          RedisModule_CallReplyType(addReply), REDISMODULE_REPLY_INTEGER);
        RedisModule_Log(ctx, "warning", "command sent: ts.add xx %lld %s", (long long)result->time, std::to_string(result->value.at(0)).c_str());
        return RedisModule_ReplyWithError(ctx, "ts.add returned a wrong reply");
      }
    }

    RedisModule_ReplyWithString(ctx, valueStr);
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
  std::cout<< "Loading redisrtbot module" << std::endl;
  if (RedisModule_Init(ctx, "rtbot", 1, REDISMODULE_APIVER_1) == REDISMODULE_ERR) return REDISMODULE_ERR;

  if (RedisModule_CreateCommand(ctx, "rtbot.run", RtBotRun_RedisCommand, "write", 0, 0, 0) == REDISMODULE_ERR)
    return REDISMODULE_ERR;

  return REDISMODULE_OK;
}
}

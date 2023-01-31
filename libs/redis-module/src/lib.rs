#![recursion_limit = "256"]

mod program_json_schema;

#[macro_use]
extern crate redis_module;

use crate::program_json_schema::ProgramJsonSchemaValidator;
use redis_module::RedisError::*;
use redis_module::{Context, RedisResult, RedisString, RedisValue};
use serde_json::Value;
use rtbot;

fn rtbot_run<'a>(ctx: &Context, args: Vec<RedisString>) -> RedisResult {
    let x = rtbot::add(1, 2);
    let mut it = args.into_iter();
    let program_key = it.next().ok_or(Str("No program key provided"))?;
    let input_key = it.next().ok_or(Str("No input key provided"))?;
    let output_key = it.next().ok_or(Str("No output key provided"))?;

    // check that the program key contains a valid program
    let program_key_str = program_key.try_as_str()?;
    let get_program_result = match ctx.call("json.get", &[program_key_str])? {
        RedisValue::SimpleString(s) => Ok(s),
        _ => Err(Str("Response from 'json.get' is not a string")),
    }?;
    ctx.log_verbose(format!("json.get result {:?}", get_program_result).as_str());

    let program_parsed = serde_json::from_str::<Value>(get_program_result.as_str())
        .map_err(|err| String(format!("Unable to parse returned json:\n {}", err)))?;
    let validator = ProgramJsonSchemaValidator::new();
    // validate the program input according to our json schema for the program
    validator
        .validate(&program_parsed)
        .map_err(|err| String(err))?;

    let greet = format!("valid {}", true);
    let response = Vec::from(greet);

    return Ok(response.into());
}

redis_module! {
    name: "rtbot",
    version: 1,
    data_types: [],
    commands: [
        ["rtbot.run", rtbot_run, "write", 0, 0, 0]
    ]
}

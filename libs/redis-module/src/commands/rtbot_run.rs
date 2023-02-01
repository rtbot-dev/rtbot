use std::ffi::CString;
use redis_module::RedisError::{Str, String};
use redis_module::{Context, RedisResult, RedisString, RedisValue};
use serde_json::Value;
use crate::pipelines_manager::{PIPELINES_MANAGER, PipelinesManager};
use crate::program_validator::ProgramJsonSchemaValidator;

pub fn run<'a>(ctx: &Context, args: Vec<RedisString>) -> RedisResult
{
    if PIPELINES_MANAGER.read().unwrap().is_none() {
        let mut manager = PIPELINES_MANAGER.write().unwrap();
        *manager = Some(PipelinesManager::new());
    }
    let mut it = args.into_iter();
    it.next(); // pop the first argument, which is the command itself
    let program_key = it.next().ok_or(Str("No program key provided"))?;
    let input_key = it.next().ok_or(Str("No input key provided"))?;
    let output_key = it.next().ok_or(Str("No output key provided"))?;

    // check that the program key contains a valid program
    let program_key_str = program_key.try_as_str()?;
    let get_program_result = match ctx.call("json.get", &[program_key_str])? {
        RedisValue::SimpleString(s) => Ok(s),
        result => Err(String(format!("Response from 'json.get' is not a string: {} -> {:?}", program_key_str, result))),
    }?;
    ctx.log_verbose(format!("json.get result {:?}", get_program_result).as_str());

    let program_parsed = serde_json::from_str::<Value>(get_program_result.as_str())
        .map_err(|err| String(format!("Unable to parse returned json:\n {}", err)))?;
    let validator = ProgramJsonSchemaValidator::new();
    // validate the program input according to our json schema for the program
    validator
        .validate(&program_parsed)
        .map_err(|err| String(err))?;

    // let's create a pipeline
    let mut binding = PIPELINES_MANAGER.write().unwrap();
    let mut manager = binding.as_mut().unwrap();

    let pipeline_id = manager.create(get_program_result, input_key.into(), output_key.into())?;
    let response = format!("pipeline id {}", pipeline_id);
    let response = Vec::from(response);

    return Ok(response.into());
}
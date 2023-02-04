use std::ffi::CString;
use redis_module::RedisError::{Str, String};
use redis_module::{Context, RedisResult, RedisString, RedisValue};
use serde_json::Value;
use crate::pipelines_registry::{PIPELINES_REGISTRY, PipelinesRegistry};
use crate::program_validator::ProgramJsonSchemaValidator;

pub fn run<'a>(ctx: &Context, args: Vec<RedisString>) -> RedisResult
{
    if PIPELINES_REGISTRY.read().unwrap().is_none() {
        let mut manager = PIPELINES_REGISTRY.write().unwrap();
        *manager = Some(PipelinesRegistry::new());
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
    // ctx.log_verbose(format!("json.get result {:?}", get_program_result).as_str());

    let program_parsed = serde_json::from_str::<Value>(get_program_result.as_str())
        .map_err(|err| String(format!("Unable to parse returned json:\n {}", err)))?;
    let validator = ProgramJsonSchemaValidator::new();
    // validate the program input according to our json schema for the program
    validator
        .validate(&program_parsed)
        .map_err(|err| String(err))?;

    // let's create a pipeline
    let mut binding = PIPELINES_REGISTRY.write().unwrap();
    let mut registry = binding.as_mut().unwrap();

    let input_key_str = input_key.to_string();
    let output_key_str = output_key.to_string();
    // create the pipeline
    let pipeline_id = registry.create(&get_program_result, &input_key_str, &output_key_str)?;
    // get the data from the input key
    let input_ts = match ctx.call("ts.range", &[input_key.to_string().as_str(), "-", "+"])? {
        RedisValue::Array(s) => Ok(s),
        result => Err(String(format!("Response from 'json.get' is not a string: {} -> {:?}", program_key_str, result))),
    }?;
    //println!("Input ts {:?}", input_ts);
    // send it to the pipeline
    for entry in input_ts {
        let row = match entry {
            RedisValue::Array(arr) => Some(arr),
            _ => None
        }.unwrap();
        let timestamp = match &row[0] {
            RedisValue::Integer(v) => Some(v),
            _ => None
        }.unwrap();
        let value = match &row[1] {
            RedisValue::SimpleString(s) => Some(s),
            _ => None
        }.unwrap().parse::<f64>().unwrap();
        let result = registry.receive(&input_key_str, *timestamp as u64, vec![value])?;
        // now for each of the registered outputs send the result
        for (registered_output_key, message) in result {
            if !message.is_empty() {
                println!("Sending message {:?} to {}", message, registered_output_key)
            }
        }
    }
    // delete the pipeline
    registry.delete(&input_key_str, &output_key_str);
    let response = format!("pipeline id {}", pipeline_id);
    let response = Vec::from(response);

    return Ok(response.into());
}
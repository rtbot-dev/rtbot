use crate::pipelines_registry::{PipelinesRegistry, PIPELINES_REGISTRY};
use crate::program_validator::ProgramJsonSchemaValidator;
use redis_module::RedisError::{Str, String};
use redis_module::{Context, RedisResult, RedisString, RedisValue};
use serde_json::Value;
use std::ffi::CString;

pub fn run<'a>(ctx: &Context, args: Vec<RedisString>) -> RedisResult {
    if PIPELINES_REGISTRY.read().unwrap().is_none() {
        let mut manager = PIPELINES_REGISTRY.write().unwrap();
        *manager = Some(PipelinesRegistry::new());
    }
    let mut it = args.into_iter();
    it.next(); // pop the first argument, which is the command itself
    let program_key = it.next().ok_or(Str("No program key provided"))?.to_string();
    let input_key = it.next().ok_or(Str("No input key provided"))?.to_string();
    let output_key = it.next().ok_or(Str("No output key provided"))?.to_string();
    let persist_pipeline = it
        .next()
        .or(Some(RedisString::create(ctx.ctx, "false")))
        .unwrap()
        .to_string()
        .parse::<bool>()?;

    println!("persist pipeline {}", persist_pipeline);
    // check that the program key contains a valid program
    let get_program_result = match ctx.call("json.get", &[&program_key])? {
        RedisValue::SimpleString(s) => Ok(s),
        result => Err(String(format!(
            "Response from 'json.get' is not a string: {} -> {:?}",
            program_key, result
        ))),
    }?;

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

    // create the pipeline
    let pipeline_id = registry.create(&get_program_result, &input_key, &output_key)?;
    // get the data from the input key
    let input_ts = match ctx.call("ts.range", &[input_key.to_string().as_str(), "-", "+"])? {
        RedisValue::Array(s) => Ok(s),
        result => Err(String(format!(
            "Response from 'json.get' is not an array: {} -> {:?}",
            program_key, result
        ))),
    }?;
    // delete whatever is in the output key, if something
    ctx.call("del", &[output_key.as_str()])?;
    // send it to the pipeline
    for entry in input_ts {
        let row = match entry {
            RedisValue::Array(arr) => Some(arr),
            _ => None,
        }
        .unwrap();
        let timestamp = match &row[0] {
            RedisValue::Integer(v) => Some(v),
            _ => None,
        }
        .unwrap();
        let value = match &row[1] {
            RedisValue::SimpleString(s) => Some(s),
            _ => None,
        }
        .unwrap()
        .parse::<f64>()
        .unwrap();
        let result = registry.receive(&input_key, *timestamp as u64, vec![value])?;
        // now for each of the registered outputs send the result
        for (registered_output_key, message) in result {
            if !message.is_empty() {
                // println!("Sending message {:?} to {}", message, registered_output_key);
                // notice that here we just consider the first element in the values as
                // the value to consider as the output to store in the output timeseries
                let timestamp = message[0].timestamp;
                let value = message[0].values[0];

                ctx.call(
                    "ts.add",
                    &[
                        registered_output_key.as_str(),
                        timestamp.to_string().as_str(),
                        value.to_string().as_str(),
                    ],
                )?;
            }
        }
    }

    if persist_pipeline {
        return Ok(pipeline_id.into());
    } else {
        // delete the pipeline
        registry.delete(&input_key, &output_key)?;
        Ok("OK".into())
    }
}

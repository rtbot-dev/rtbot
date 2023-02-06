use crate::cxx_bindings::ffi::RtBotMessage;
use crate::pipelines_registry::PIPELINES_REGISTRY;
use redis_module::{Context, NotifyEvent, RedisError, RedisResult, RedisString, RedisValue};
use std::collections::BTreeMap;

pub fn on_generic_event(ctx: &Context, _event_type: NotifyEvent, _event: &str, _key: &str) {
    if _event == "ts.add" {
        let last = match ctx.call("ts.get", &[_key]).unwrap() {
            RedisValue::Array(arr) => arr,
            _ => {
                ctx.log_warning("Response from 'ts.get' is not an array");
                return;
            }
        };

        let timestamp = match &last[0] {
            RedisValue::Integer(v) => Some(v),
            _ => None,
        }
        .unwrap();
        let value = match &last[1] {
            RedisValue::SimpleString(s) => Some(s),
            _ => None,
        }
        .unwrap()
        .parse::<f64>()
        .unwrap();
        // and send it to the pipelines registered for the key
        let binding = PIPELINES_REGISTRY.read().unwrap();
        let registry = binding.as_ref().unwrap();
        let result = registry
            .receive(_key, *timestamp as u64, vec![value])
            .unwrap();
        // now for each of the registered outputs send the result
        for (registered_output_key, message) in result {
            if !message.is_empty() {
                // println!("Sending message {:?} to {}", message, registered_output_key);
                // notice that here we just consider the first element in the values as
                // the value to consider as the output to store in the output timeseries
                let timestamp = message[0].timestamp;
                let value = message[0].values[0];

                match ctx.call(
                    "ts.add",
                    &[
                        registered_output_key.as_str(),
                        timestamp.to_string().as_str(),
                        value.to_string().as_str(),
                    ],
                ) {
                    Ok(_) => {}
                    Err(_) => {
                        ctx.log_warning(
                            format!("Unable to send message to {}", registered_output_key).as_str(),
                        );
                        return;
                    }
                };
            }
        }
    }
}

pub fn run<'a>(ctx: &Context, args: Vec<RedisString>) -> RedisResult {
    let mut it = args.into_iter();
    it.next(); // pop the first argument, which is the command itself
    let program_key = it
        .next()
        .ok_or(RedisError::Str("No program key provided"))?
        .to_string();
    let input_key = it
        .next()
        .ok_or(RedisError::Str("No input key provided"))?
        .to_string();
    let output_key = it
        .next()
        .ok_or(RedisError::Str("No output key provided"))?
        .to_string();

    // run the usual program, but requiring a persistent pipeline
    ctx.call(
        "rtbot.run",
        &[&program_key, &input_key, &output_key, "true"],
    )
}

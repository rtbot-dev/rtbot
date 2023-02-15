use crate::cxx_bindings::ffi::RtBotMessage;
use crate::pipelines_registry::PIPELINES_REGISTRY;
use redis_module::RedisError::Str;
use redis_module::{Context, NotifyEvent, RedisError, RedisResult, RedisString, RedisValue};
use std::collections::BTreeMap;

pub fn on_generic_event(
    ctx: &Context,
    _event_type: NotifyEvent,
    _event: &str,
    _key: &str,
) -> RedisResult {
    if _event == "ts.add" {
        let last = if let RedisValue::Array(arr) = ctx.call("ts.get", &[_key]).unwrap() {
            arr
        } else {
            return Err(Str("Response from 'ts.get' is not an array"));
        };

        let timestamp = if let RedisValue::Integer(ts) = &last[0] {
            ts
        } else {
            return Err(Str(
                "Response from 'ts.get' first element is not a valid timestamp",
            ));
        };
        let value = if let RedisValue::SimpleString(s) = &last[1] {
            s.parse::<f64>().unwrap()
        } else {
            return Err(Str(
                "Response from 'ts.get' first element is not a valid 64 bit float",
            ));
        };
        // and send it to the pipelines registered for the key
        let binding = PIPELINES_REGISTRY.read().unwrap();
        let registry = binding.as_ref().unwrap();
        let result = registry
            .receive(_key, *timestamp as u64, vec![value])
            .unwrap();
        // now for each of the registered outputs send the result
        for (registered_output_key, message) in result {
            if !message.is_empty() {
                // notice that here we just consider the first element in the values as
                // the value to consider as the output to store in the output timeseries
                let timestamp = message[0].timestamp;
                let value = message[0].values[0];
                if let Err(msg) = ctx.call(
                    "ts.add",
                    &[
                        registered_output_key.as_str(),
                        timestamp.to_string().as_str(),
                        value.to_string().as_str(),
                    ],
                ) {
                    ctx.log_warning(
                        format!(
                            "Adding pipeline output to {} failed: {:?}",
                            registered_output_key.to_string(),
                            msg
                        )
                        .as_str(),
                    );
                };
                // send the saved message to the correspondent output pubsub channel
                if let Err(msg) = ctx.call(
                    "publish",
                    &[
                        // channel
                        format!("{}:ps", registered_output_key).as_str(),
                        // value: a comma separated string where the first element is the timestamp
                        // and the remaining ones the values
                        format!("{},{}", message[0].timestamp, message[0].values[0]).as_str(),
                    ],
                ) {
                    ctx.log_warning(
                        format!(
                            "Publishing pipeline output to {}:ps failed: {:?}",
                            registered_output_key.to_string(),
                            msg
                        )
                        .as_str(),
                    );
                };
            }
        }
    }
    Ok(RedisValue::from(()))
}

pub fn run<'a>(ctx: &Context, args: Vec<RedisString>) -> RedisResult {
    let mut it = args.into_iter();
    it.next(); // pop the first argument, which is the command itself
    let program_key = it.next().ok_or(Str("No program key provided"))?.to_string();
    let input_key = it.next().ok_or(Str("No input key provided"))?.to_string();
    let output_key = it.next().ok_or(Str("No output key provided"))?.to_string();

    // run the usual program, but requiring a persistent pipeline
    ctx.call(
        "rtbot.run",
        &[&program_key, &input_key, &output_key, "true"],
    )
}

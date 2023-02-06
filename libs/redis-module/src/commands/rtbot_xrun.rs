use crate::pipelines_registry::{PipelinesRegistry, PIPELINES_REGISTRY};
use redis_module::RedisError::Str;
use redis_module::{Context, NotifyEvent, RedisResult, RedisString};

pub fn on_generic_event(ctx: &Context, _event_type: NotifyEvent, _event: &str, _key: &str) {
    if _event == "ts.add" {
        // println!("ts.add received on key {}", _key)
        // TODO: get the latest element added
        // and send it to the pipelines registered for the key
    }
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

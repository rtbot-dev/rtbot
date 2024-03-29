#![recursion_limit = "256"]

use redis_module::RedisError;
use rtbot_rs::pipelines_registry::PipelineError;
use rtbot_rs::*;
mod commands;
mod program_validator;

#[macro_use]
extern crate redis_module;

redis_module! {
    name: "rtbot",
    version: 1,
    data_types: [],
    commands: [
        ["rtbot.run", commands::rtbot_run::run, "write", 0, 0, 0],
        ["rtbot.xrun", commands::rtbot_xrun::run, "write", 0, 0, 0]
    ],
    event_handlers: [
        [@ALL: commands::rtbot_xrun::on_generic_event]
    ]
}

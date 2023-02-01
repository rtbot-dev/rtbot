#![recursion_limit = "256"]

mod program_validator;
mod commands;
mod pipelines_manager;

#[macro_use]
extern crate redis_module;

redis_module! {
    name: "rtbot",
    version: 1,
    data_types: [],
    commands: [
        ["rtbot.run", commands::rtbot_run::run, "write", 0, 0, 0]
    ]
}

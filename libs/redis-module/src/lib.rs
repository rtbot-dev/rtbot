#![recursion_limit = "256"]

mod program_validator;
mod commands;
mod pipelines_registry;
mod cxx_bindings;

#[macro_use]
extern crate redis_module;

redis_module! {
    name: "rtbot",
    version: 1,
    data_types: [],
    commands: [
        ["rtbot.run", commands::rtbot_run::run, "write", 0, 0, 0]
    ],
    event_handlers: [
        [@ALL: commands::rtbot_xrun::on_generic_event]
    ]
}

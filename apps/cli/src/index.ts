import { Command } from "commander";
import { registerDebug } from "./commands/debug";

const program = new Command();

program.name("rtbot").description("RtBot command line interface.").version("0.1.0");

// register subcommands
registerDebug(program);

// run the program
program.parse();

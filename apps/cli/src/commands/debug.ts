import { Command } from "commander";
import { readFileSync, writeFileSync } from "fs";
import Papa, { ParseResult } from "papaparse";
import { Program, RtBotRun, RtBotRunOutputFormat, ExtendedFormat } from "@rtbot/api";
import { tui } from "../tui";

export const registerDebug = (program: Command) => {
  program
    .command("debug")
    .description(
      `Runs an RtBot program and produces a debug output.
      This output is a list of outputs produced by operators that emmitted
      in a given iteration. An iteration happens when we send a message to 
      the program.
      `
    )
    .argument(
      "programFile",
      "A path to a file where the RtBot program is. It is assumed by default that it is in json format."
    )
    .argument("inputData", "A file with the input data that will be passed to the program. Expects csv format.")
    .option(
      "-st, --scale-time-factor <number>",
      "A number that specifies the scale factor for time. Useful to change time units",
      "1"
    )
    .option(
      "-sv, --scale-value-factor <number>",
      "A number that specifies the scale factor for the values. Useful to change value units",
      "1"
    )
    .option("-i, --interactive", "If set, shows an interactive screen useful for debugging programs.")
    .option("-o, --output <string>", "If provided, indicates to which file the program debug output should be written.")
    .option("-v, --verbose", "Show extra information while running the program", false)
    .action(async (programFile, inputData, { output, verbose, interactive, scaleTimeFactor, scaleValueFactor }) => {
      if (!inputData) {
        console.error("Please provide an input file in csv format.");
        process.exit(1);
      }
      if (verbose && output) {
        console.log("Program output will be written in", output);
      }
      if (verbose) console.log("Loading program from", programFile);
      let rtBotProgramContent = "";

      try {
        rtBotProgramContent = readFileSync(programFile, { encoding: "utf8" });
      } catch (e: any) {
        console.error("Unable to read file", programFile, "reason:", e.message);
      }

      if (verbose) console.log("program loaded, title:", JSON.parse(rtBotProgramContent).title);

      let data: ParseResult<any>;

      try {
        data = await new Promise((resolve, reject) => {
          Papa.parse(readFileSync(inputData, { encoding: "utf8" }), {
            worker: true,
            complete(result) {
              resolve(result);
            },
            error(error: Error) {
              reject(error);
            },
          });
        });
      } catch (e: any) {
        console.log("Unable to read data from", inputData, ", reason:", e.message);
      }

      if (verbose) console.log("Data loaded, number of rows:", data!.data.length);
      const rtBotProgram = Program.toInstance(JSON.parse(rtBotProgramContent));
      rtBotProgram.validate();
      if (verbose)
        console.log(
          `RtBot program parsed and validated, operators: ${rtBotProgram.operators.length}, connections: ${rtBotProgram.connections.length}`
        );

      // now let's send the data to the program
      if (verbose) console.log("Scale time factor:", scaleTimeFactor);
      const preparedData = data!.data
        .map(([t, v]) => {
          try {
            // TODO: extends this logic later to allow several inputs
            // by now we just consider the first column as time and the second
            // one as the only value we want to send
            const time = Math.round(scaleTimeFactor * parseFloat(t));
            const value = scaleValueFactor * parseFloat(v);
            if (isNaN(time) || isNaN(value)) return [];
            return [time, value];
          } catch (e: any) {
            return [];
          }
        })
        .filter((r) => r.length === 2);
      const rtbotRun = new RtBotRun(rtBotProgram, preparedData, RtBotRunOutputFormat.EXTENDED, verbose);
      await rtbotRun.run();
      const result = rtbotRun.getOutputs();
      // finally dump the result
      if (interactive) {
        // show the interactive screen
        tui(rtBotProgram, result as ExtendedFormat);
      } else {
        const resultContent = JSON.stringify(result, null, 2);
        if (output) {
          writeFileSync(output, resultContent, { encoding: "utf8" });
          console.log(`Done. Program output written to ${output}.`);
        } else console.log(resultContent);
      }
    });
};

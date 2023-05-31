import chalk from "chalk";
import { Runfiles } from "@bazel/runfiles";
import { compileTemplate, registerPartial } from "./handlebars";
const { Command } = require("commander");
const fs = require("fs");

const log = console.log;

const program = new Command();

registerPartial("operator");
const mainTemplate = compileTemplate("main");

program
  .description("RtBot python wrapper generator")
  .option("-f, --files [files...]", "A list of files of RtBot core c++")
  .option("-o,--output <string>", "Output directory")
  .action(async ({ output, files }) => {
    if (files.length === 1) files = files[0].split(" ");
    log(
      `${chalk.cyan("Generating python wrapper, scanning c++ input files:\n  - ")}${chalk.yellow(files.join("\n  - "))}`
    );
    let content = "";
    let models = await Promise.all(
      files.map(async (f) => {
        try {
          console.log("f", f);
          const fileContent = await fs.readFileSync(f).toString();
          // check if the file has an operator defined inside
          const hasOperatorRegex = /(struct|class) (.*) : public (Buffer|Join|Operator)<T, V>/gm;
          const hasOperatorTest = Array.from(fileContent.matchAll(hasOperatorRegex));
          if (hasOperatorTest.length > 0 && (hasOperatorTest[0] as string[]).length > 2) {
            const op = hasOperatorTest[0][2];
            log(chalk.green(`Found operator: ${op}`));
            // now get the constructor parameters
            const constructorParameters = fileContent
              .split(`${op}(`)
              .slice(1)
              .map((p) => p.split(")")[0])
              .reduce((acc, val) => (val.length > acc.length ? val : acc));
            console.log("constructor parameters", constructorParameters);
            return {
              operator: op,
              types: constructorParameters.split(",").map((p) =>
                p
                  .split(" ")
                  .slice(0, -1)
                  .map((p) => p.trim())
                  .join(" ")
              ),
            };
          }
          return false;
        } catch (e) {
          log(chalk.red(`Error: ${e.message}`));
        }
      })
    );
    models = models.filter((m) => !!m);
    console.log("models", models);
    content = mainTemplate({ models });

    fs.writeFileSync(`${output}/lib.cpp`, content);
  });

program.parseAsync();

import chalk from "chalk";
import { compileTemplate, registerPartial } from "./handlebars";
const { Command } = require("commander");
const fs = require("fs");
const toJsonSchema = require("to-json-schema");

const log = console.log;

const program = new Command();

registerPartial("operator");
const mainTemplate = compileTemplate("main");

program
  .description("RtBot json schema generator")
  .option("-f, --files [files...]", "A list of files of RtBot core c++")
  .option("-o,--output <string>", "Output directory")
  .action(async ({ output, files }) => {
    if (files.length === 1) files = files[0].split(" ");
    log(`${chalk.cyan("Generating schemas, scanning c++ input files:\n  - ")}${chalk.yellow(files.join("\n  - "))}`);
    let schemas = await Promise.all(
      files.map(async (f: string) => {
        let schemas = [];
        try {
          const fileContent = await fs.readFileSync(f).toString();
          // check if the file has an operator defined inside
          const multilineCommentRegex = /\*([^*]|[\r\n]|(\*+([^*\/]|[\r\n])))*\*+/gm;
          schemas = Array.from(fileContent.matchAll(multilineCommentRegex)).map((groups: string[]) =>
            groups
              .filter((g) => g && g.indexOf("{") > -1)
              .map((g) => {
                let opSampleCode = undefined;
                try {
                  opSampleCode = JSON.parse(g.replaceAll("*", ""));
                  opSampleCode.opType = opSampleCode.type;
                  delete opSampleCode.type;
                } catch (e) {}
                return opSampleCode;
              })
          );
        } catch (e) {
          log(chalk.red(`Error: ${e.message}`));
        }
        // return the schemas found, after flattening the list
        return schemas.reduce((acc, v) => [...acc, ...v], []);
      })
    );

    // continue flattening the schema list and generate the jsonschemas from examples
    schemas = schemas
      .reduce((acc, v) => [...acc, ...v], [])
      .filter((s: any) => s)
      .map((s: any) => {
        const opType = s.opType;
        return JSON.stringify(
          toJsonSchema(s, {
            objects: {
              postProcessFnc: (schema: any, obj: any, defaultFnc: any) => {
                const s = defaultFnc(schema, obj);
                // make opType a const
                // do this only at the level where the id prop is present
                if (s.properties.id) {
                  s.properties.opType = {
                    enum: [opType],
                  };
                }
                return s;
              },
            },
          })
        );
      });
    const content = JSON.stringify(JSON.parse(mainTemplate({ schemas })), null, 2);

    fs.writeFileSync(`${output}/jsonschema.json`, content);
  });

program.parseAsync();

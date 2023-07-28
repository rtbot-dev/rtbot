import chalk from "chalk";
import { compileTemplate } from "./handlebars";
import { Command, Option } from "commander";
import { compile } from "json-schema-to-typescript";
import { parseSchema } from "json-schema-to-zod";
import { parse } from "yaml";

const fs = require("fs");
const toJsonSchema = require("to-json-schema");
const prettier = require("prettier");

const program = new Command();

const jsonschemaTemplate = compileTemplate("jsonschema");
const typescriptTemplate = compileTemplate("typescript");
const JSONSCHEMA_TAG = "@jsonschema";

program
  .description("RtBot code generator")
  .option("-s, --sources [sources...]", "A list of files of RtBot core c++")
  .option("-o, --output <string>", "Output directory")
  .addOption(new Option("-t, --target <target>", "Target output format").choices(["jsonschema", "typescript"]))
  .action(async ({ output, sources, target }) => {
    if (sources.length === 1) sources = sources[0].split(" ");
    console.log(
      `${chalk.cyan("Generating schemas, scanning c++ input files:\n  - ")}${chalk.yellow(sources.join("\n  - "))}`
    );
    let schemas = await Promise.all(
      sources.map(async (f: string) => {
        try {
          const fileContent = await fs.readFileSync(f).toString();
          if (fileContent.indexOf(JSONSCHEMA_TAG) > -1) {
            const schemaStr = fileContent
              .split(JSONSCHEMA_TAG)[1]
              .split("*/")[0]
              .replaceAll(" * ", "")
              .replaceAll(" *\n", "\n");
            const schema = parse(schemaStr);
            const opType = fileContent.split("string typeName()")[1].split('return "')[1].split('"')[0];
            schema.properties.opType = { enum: [opType] };
            schema.required = ["opType", ...schema.required];
            return schema;
          }
        } catch (e) {
          console.log(chalk.red(`Error: ${e.message}, file ${f}`));
        }
      })
    );
    schemas = schemas.filter((s) => s);

    const programJsonschema = JSON.parse(jsonschemaTemplate({ schemas: schemas.map((s) => JSON.stringify(s)) }));
    const jsonschemaContent = JSON.stringify(programJsonschema, null, 2);

    fs.writeFileSync(`${output}/jsonschema.json`, jsonschemaContent);

    if (target === "typescript") {
      const typescriptContent = typescriptTemplate({
        schemas: parseSchema({ type: "array", items: programJsonschema.properties.operators.items.oneOf })
          .replace("z.tuple(", "")
          .slice(0, -1),
        operators: await Promise.all(
          schemas.map(async (schema) => {
            const properties = Object.keys(schema.properties);
            const opType = schema.properties.opType.enum[0];
            const ts = await compile(schema, opType);
            let parametersBlock = ts
              .split("export interface")[1]
              .split("\n")
              .filter((l) => l.indexOf("opType") === -1 && l.indexOf("unknown") === -1)
              .slice(1)
              .slice(0, -2)
              .map((l) => l.replace(";", ","))
              .join("\n");
            properties.forEach(
              (prop) =>
                (parametersBlock = parametersBlock
                  .replace(`${prop}:`, `readonly ${prop}:`)
                  .replace(`${prop}?:`, `readonly ${prop}?:`))
            );
            const zodSchema = parseSchema(schema);

            return {
              opType,
              parametersBlock,
              schema: zodSchema,
            };
          })
        ),
      });
      fs.writeFileSync(`${output}/index.ts`, prettier.format(typescriptContent, { parser: "babel-ts" }));
    }
  });

program.parseAsync();

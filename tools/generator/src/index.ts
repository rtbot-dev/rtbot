import chalk from "chalk";
import { compileTemplate } from "./handlebars";
import { Command, Option } from "commander";
import { compile } from "json-schema-to-typescript";
import { parseSchema } from "json-schema-to-zod";
import { parse } from "yaml";

const fs = require("fs");
const prettier = require("prettier");
const path = require("path");

const program = new Command();

const jsonschemaTemplate = compileTemplate("jsonschema");
const typescriptTemplate = compileTemplate("typescript");
const pythonTemplate = compileTemplate("python");

program
  .description("RtBot code generator")
  .option(
    "-s, --sources [sources...]",
    "A list of markdown files where the documentation and schema of the operators are stored"
  )
  .option("-o, --output <string>", "Output directory")
  .addOption(
    new Option("-t, --target <target>", "Target output format").choices(["jsonschema", "typescript", "cpp", "python"])
  )
  .action(async ({ output, sources, target }) => {
    if (sources.length === 1) sources = sources[0].split(" ");
    console.log(
      `${chalk.cyan("Generating schemas, scanning c++ input files:\n  - ")}${chalk.yellow(sources.join("\n  - "))}`
    );
    let schemas = await Promise.all(
      sources.map(async (f: string) => {
        try {
          const fileContent = await fs.readFileSync(f).toString();
          if (fileContent.indexOf("---") > -1) {
            const schemaStr = fileContent.split("---")[1];
            const schema = parse(schemaStr).jsonschema;
            const type = path.basename(f).replace(".md", "");
            schema.properties.type = { enum: [type] };
            schema.required = ["type", ...schema.required];
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

    if (target === "cpp") {
      fs.writeFileSync(
        `${output}/jsonschema.hpp`,
        `#include <nlohmann/json.hpp>

        using nlohmann::json;

        static json rtbot_schema = R"(
          ${jsonschemaContent}
        )"_json;`
      );
    }

    if (target === "python") {
      const opSchemas = programJsonschema.properties.operators.items.oneOf;
      const pythonContent = pythonTemplate({
        operators: opSchemas.map((s: any) => ({
          type: s.properties.type.enum[0],
          parameters: Object.keys(s.properties)
            .filter((p) => p !== "type")
            .map((k) => ({
              name: k,
              init: s.required.indexOf(k) > -1 ? "" : ` = ${s.properties[k].default ?? "None"}`,
            })),
        })),
      });
      fs.writeFileSync(`${output}/jsonschema.py`, pythonContent);
    }

    if (target === "typescript") {
      const typescriptContent = typescriptTemplate({
        schemas: parseSchema({ type: "array", items: programJsonschema.properties.operators.items.oneOf })
          .replace("z.tuple(", "")
          .slice(0, -1),
        operators: await Promise.all(
          schemas.map(async (schema) => {
            const properties = Object.keys(schema.properties);
            const type = schema.properties.type.enum[0];
            const ts = await compile(schema, type);
            let parametersBlock = ts
              .split("export interface")[1]
              .split("\n")
              .filter((l) => l.indexOf("type") === -1 && l.indexOf("unknown") === -1)
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
              type,
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

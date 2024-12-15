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
const usageMdTemplate = compileTemplate("usage-md");
const parametersMdTemplate = compileTemplate("parameters-md");

program
  .description("RtBot code generator")
  .option(
    "-s, --sources [sources...]",
    "A list of markdown files where the documentation and schema of the operators are stored"
  )
  .option("-o, --output <string>", "Output directory")
  .addOption(
    new Option("-t, --target <target>", "Target output format").choices([
      "jsonschema",
      "typescript",
      "cpp",
      "python",
      "markdown",
    ])
  )
  .action(async ({ output, sources, target }) => {
    if (sources.length === 1) sources = sources[0].split(" ");
    console.log(`${chalk.cyan("[schemas] scanning c++ input files: ")}${chalk.yellow(sources.length)}`);
    let schemas = await Promise.all(
      sources.map(async (f: string) => {
        try {
          const fileContent = await fs.readFileSync(f).toString();
          if (fileContent.indexOf("---") > -1) {
            const headerStr = fileContent.split("---")[1];
            const header = parse(headerStr);
            const schema = header.jsonschema;
            if (schema) {
              const type = path.basename(f).replace(".md", "");
              schema.properties.type = { enum: [type] };
              schema.required = ["type", ...schema.required];
              return schema;
            }
            const schemas = header.jsonschemas;
            if (schemas) {
              return schemas.map((s: any) => ({
                ...s,
                required: ["type", ...s.required],
              }));
            }
          }
        } catch (e) {
          console.log(chalk.red(`Error: ${e.message}, file ${f}`));
        }
      })
    );
    schemas = schemas
      .filter((s) => s)
      // flatten the array of schemas
      .reduce((acc, s) => acc.concat(s), []);
    console.log(
      `${chalk.cyan("[schemas] found operators:\n  - ")}${chalk.yellow(
        schemas.map((s) => s.properties.type.enum[0]).reduce((acc, s) => `${acc}\n  - ${s}`)
      )}`
    );

    const programJsonschema = JSON.parse(jsonschemaTemplate({ schemas: schemas.map((s) => JSON.stringify(s)) }));
    const jsonschemaContent = JSON.stringify(programJsonschema, null, 2);

    fs.writeFileSync(`${output}/jsonschema.json`, jsonschemaContent);

    if (target === "markdown") {
      sources.map(async (f: string) => {
        try {
          let fileContent = await fs.readFileSync(f).toString();
          if (fileContent.indexOf("---") > -1) {
            const frontMatterStr = fileContent.split("---")[1];
            const frontMatter = parse(frontMatterStr);
            const schema = frontMatter.jsonschema;
            // cook up an example parameter object
            const getExampleParameter = (k: string) => {
              if (k === "id") return '"id"';
              if (schema.properties[k].examples) {
                if (schema.properties[k].type === "array") return `[${schema.properties[k].examples[0]}]`;
                return schema.properties[k].examples[0];
              }
              if (schema.properties[k].type === "integer") return 2;
              if (schema.properties[k].type === "numeric") return 2.0;
              if (schema.properties[k].type === "string") return "some";
              if (schema.properties[k].type === "boolean") return true;
            };
            const exampleParameters = Object.keys(schema.properties).reduce(
              (acc, k) => ({ ...acc, [`${k}`]: getExampleParameter(k) }),
              {}
            );
            const opParams = Object.keys(exampleParameters).join(", ");
            const opParamsDef = Object.keys(exampleParameters).reduce(
              (acc, k) => `${acc}const ${k} = ${exampleParameters[k]};\n`,
              ""
            );
            const opParamsDefPy = Object.keys(exampleParameters).reduce(
              (acc, k) => `${acc}${k} = ${exampleParameters[k]};\n`,
              ""
            );
            const opParamsDefCpp = Object.keys(exampleParameters)
              .reduce((acc, k) => `${acc}auto ${k} = ${exampleParameters[k]};\n`, "")
              .replaceAll("[", "{")
              .replaceAll("]", "}");
            const opParamsYaml = Object.keys(exampleParameters).reduce(
              (acc, k) => `${acc}    ${k}: ${exampleParameters[k]}\n`,
              ""
            );
            const opParamsJson = Object.keys(exampleParameters)
              .reduce((acc, k) => [...acc, `"${k}": ${exampleParameters[k]}`], [])
              .join(", ");

            const op = path.basename(f).replace(".md", "");
            const usageSection = usageMdTemplate({
              op,
              opParams,
              opParamsDef,
              opParamsDefPy,
              opParamsDefCpp,
              opParamsYaml,
              opParamsJson,
            });
            const parameters = {
              parameters: Object.entries(schema.properties).map(([k, v]: [string, any]) => ({ name: k, ...v })),
            };
            const parametersSection = parametersMdTemplate(parameters);
            fileContent += "\n\n" + parametersSection;
            fileContent += "\n\n" + usageSection;

            fs.writeFileSync(`${output}/${path.basename(f)}x`, fileContent);
          }
        } catch (e) {
          console.log(chalk.red(`Error: ${e.message}, file ${f}`), e.stack);
        }
      });
    }

    if (target === "cpp") {
      fs.writeFileSync(
        `${output}/jsonschema.hpp`,
        `#ifndef RTBOT_JSONSCHEMA_H_
        #define RTBOT_JSONSCHEMA_H_

        #include <nlohmann/json.hpp>

        using nlohmann::json;

        static json rtbot_schema = ""
          ${jsonschemaContent
            .split("\n")
            .map((l) => '"' + l.replaceAll('"', '\\"') + '"')
            .join("\n")}
        ""_json;
        #endif`.replace(/       +/g, "")
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
              init:
                s.required.indexOf(k) > -1
                  ? ""
                  : ` = ${s.properties[k].default ?? "None"}`.replace("true", "True").replace("false", "False"),
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
      const formatted = await prettier.format(typescriptContent, { parser: "babel-ts" });
      fs.writeFileSync(`${output}/index.ts`, formatted);
    }
  });

program.parseAsync();

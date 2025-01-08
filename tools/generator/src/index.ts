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
    "A list of markdown files where the documentation and schema of the operators are stored",
  )
  .option("-o, --output <string>", "Output directory")
  .addOption(
    new Option("-t, --target <target>", "Target output format").choices([
      "jsonschema",
      "typescript",
      "cpp",
      "python",
      "markdown",
    ]),
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
      }),
    );
    schemas = schemas
      .filter((s) => s)
      // flatten the array of schemas
      .reduce((acc, s) => acc.concat(s), []);
    console.log(
      `${chalk.cyan("[schemas] found operators:\n  - ")}${chalk.yellow(
        schemas.map((s) => s.properties.type.enum[0]).reduce((acc, s) => `${acc}\n  - ${s}`),
      )}`,
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
            const schemas = frontMatter.jsonschemas || [frontMatter.jsonschema];

            // Determine if this file has multiple schemas
            const hasMultipleSchemas = Array.isArray(schemas) && schemas.length > 1;

            let newFileContent = fileContent;

            schemas.forEach((schema: any) => {
              if (!schema) return;

              // Use the filename as the operator type if `type` is not specified
              const op = schema.properties?.type?.enum?.[0] || path.basename(f).replace(".md", "");

              // Generate the parameters section
              const parameters = {
                parameters: Object.entries(schema.properties).map(([k, v]: [string, any]) => ({ name: k, ...v })),
              };
              const parametersSection = parametersMdTemplate(parameters);

              // Append the parameters section to the file content
              // deprecated
              // newFileContent += "\n\n" + parametersSection;

              // For single schema files, inject the usage section
              if (!hasMultipleSchemas) {
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
                  {},
                );

                const opParams = Object.keys(exampleParameters).join(", ");
                const opParamsDef = Object.keys(exampleParameters).reduce(
                  (acc, k) => `${acc}const ${k} = ${exampleParameters[k]};\n`,
                  "",
                );
                const opParamsDefPy = Object.keys(exampleParameters).reduce(
                  (acc, k) => `${acc}${k} = ${exampleParameters[k]};\n`,
                  "",
                );
                const opParamsDefCpp = Object.keys(exampleParameters)
                  .reduce((acc, k) => `${acc}auto ${k} = ${exampleParameters[k]};\n`, "")
                  .replaceAll("[", "{")
                  .replaceAll("]", "}");
                const opParamsYaml = Object.keys(exampleParameters).reduce(
                  (acc, k) => `${acc}    ${k}: ${exampleParameters[k]}\n`,
                  "",
                );
                const opParamsJson = Object.keys(exampleParameters)
                  .reduce((acc, k) => [...acc, `"${k}": ${exampleParameters[k]}`], [])
                  .join(", ");

                const usageSection = usageMdTemplate({
                  op,
                  opParams,
                  opParamsDef,
                  opParamsDefPy,
                  opParamsDefCpp,
                  opParamsYaml,
                  opParamsJson,
                });

                // deprecated
                //newFileContent += "\n\n" + usageSection;
              }
            });

            // Write the file with the same name as the original
            const outputFileName = `${output}/${path.basename(f)}x`;
            fs.writeFileSync(outputFileName, newFileContent);
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
        #endif`.replace(/       +/g, ""),
      );
    }

    if (target === "python") {
      const opSchemas = programJsonschema.properties.operators.items.oneOf;
      const pythonContent = pythonTemplate({
        operators: opSchemas
          .map((s: any) => {
            if (!s.properties.type?.enum) {
              return;
            }

            return {
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
            };
          })
          .filter((s) => s),
      });
      fs.writeFileSync(`${output}/jsonschema.py`, pythonContent);
    }

    if (target === "typescript") {
      const getTypeString = (prop: any): string => {
        if (prop.name === "outputMappings") {
          return "any";
        }
        if (prop.type === "array") {
          if (
            prop.items?.enum?.every((e: string) => ["number", "boolean", "vector_number", "vector_boolean"].includes(e))
          ) {
            return "PortType[]";
          }
          if (prop.items?.enum) {
            const enumVals = prop.items.enum.map((e: string) => `"${e}"`).join(" | ");
            return `(${enumVals})[]`;
          }
          return "any[]";
        }
        if (prop.type === "string" && prop.enum) {
          if (prop.enum.every((e: string) => ["number", "boolean", "vector_number", "vector_boolean"].includes(e))) {
            return "PortType";
          }
          return prop.enum.map((e: string) => `"${e}"`).join(" | ");
        }
        const typeMap: Record<string, string> = {
          integer: "number",
          number: "number",
          string: "string",
          boolean: "boolean",
          object: "Record<string, any>",
          array: "any[]",
        };
        return typeMap[prop.type] || "any";
      };

      const nonPrototypeSchemas = programJsonschema.properties.operators.items.oneOf.filter(
        (schema: any) => !schema.properties?.prototype,
      );

      const typescriptContent = typescriptTemplate({
        schemas: nonPrototypeSchemas.map((schema) => parseSchema(schema)).join(",\n"),
        operators: schemas
          .filter((schema) => !schema.properties?.prototype)
          .map((schema) => {
            const type = schema.properties.type.enum[0];
            const props = Object.entries(schema.properties)
              .filter(([k]) => k !== "type")
              .map(([name, prop]: [string, any]) => {
                const description = prop.description ? `/**\n* ${prop.description}\n*/\n` : "";
                const typeStr = getTypeString({ ...prop, name });
                return `${description}readonly ${name}${schema.required?.includes(name) ? "" : "?"}: ${typeStr},`;
              })
              .join("\n");

            return {
              type,
              parametersBlock: props,
              schema: parseSchema(schema),
            };
          }),
      });

      const formatted = typescriptContent; //await prettier.format(typescriptContent, { parser: "babel-ts" });
      fs.writeFileSync(`${output}/index.ts`, formatted);
    }
  });

program.parseAsync();

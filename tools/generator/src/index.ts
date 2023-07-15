import chalk from "chalk";
import { compileTemplate } from "./handlebars";
import { Command, Option } from "commander";
const fs = require("fs");
const toJsonSchema = require("to-json-schema");
const prettier = require("prettier");
import { compile } from "json-schema-to-typescript";
import { parseSchema } from "json-schema-to-zod";

const program = new Command();

const jsonschemaTemplate = compileTemplate("jsonschema");
const typescriptTemplate = compileTemplate("typescript");

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
          console.log(chalk.red(`Error: ${e.message}`));
        }
        // return the schemas found, after flattening the list
        return schemas.reduce((acc, v) => [...acc, ...v], []);
      })
    );

    // continue flattening the schema list and generate the jsonschemas from examples
    schemas = await Promise.all(
      schemas
        .reduce((acc, v) => [...acc, ...v], [])
        .filter((s: any) => s)
        .map(async (s: any) => {
          const opType = s.opType;
          const schema = toJsonSchema(s, {
            objects: {
              postProcessFnc: (schema: any, obj: any, defaultFnc: any) => {
                const s = defaultFnc(schema, obj);
                s.required = Object.keys(s.properties); //.filter((p) => p !== "opType");
                s.additionalProperties = false;
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
          });

          return schema;
        })
    );
    const programJsonschema = JSON.parse(jsonschemaTemplate({ schemas: schemas.map((s) => JSON.stringify(s)) }));
    const jsonschemaContent = JSON.stringify(programJsonschema, null, 2);

    fs.writeFileSync(`${output}/jsonschema.json`, jsonschemaContent);

    if (target === "typescript") {
      const typescriptContent = typescriptTemplate({
        schemas: parseSchema({ type: "array", items: programJsonschema.properties.operators.items.oneOf }).replace("z.tuple(", "").slice(0, -1),
        operators: await Promise.all(
          schemas.map(async (schema) => {
            const opType = schema.properties.opType.enum[0];
            const ts = await compile(schema, opType);
            // parse the code between the braces
            let bracesCounter = 0;
            let parametersBlock = Array.from(ts)
              .reduce((acc, v) => {
                if (v === "{") bracesCounter += 1;
                if (v === "}") bracesCounter -= 1;

                return bracesCounter && bracesCounter > 0 ? [...acc, v] : acc;
              }, [])
              .join("")
              // drop the fisrt "{"
              .slice(1)
              // now remove the lines we don't want
              .split("\n")
              .filter((l) => l.indexOf("opType") < 0)
              .filter((l) => l.indexOf("unknown") < 0)
              .filter((l) => l.length > 0)
              .map((l) => `  ${l.replaceAll(";", ",")}`)
              .join("\n");

            parametersBlock = addParametersPrefix(parametersBlock);
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

const addParametersPrefix = (parametersBlock: string) => {
  let bracesDepth = 0;

  // find the root colons positions in string
  const rootColonPositions = Array.from(parametersBlock).reduce((acc, c, i) => {
    if (c === "{") bracesDepth += 1;
    if (c === "}") bracesDepth -= 1;

    return c === ":" && bracesDepth === 0 ? [...acc, i] : acc;
  }, []);
  // now find the position where the word previous to the root colon starts
  const parameterStarts = rootColonPositions.map((p) => {
    for (let i = p; i > -1; i--) {
      if (parametersBlock[i] === " ") return i;
    }
    return 0;
  });
  let shift = 0;
  const prefix = " readonly";
  let output = parametersBlock;
  for (let i = 0; i < parameterStarts.length; i++) {
    const index = parameterStarts[i] + shift;
    output = output.slice(0, index) + prefix + output.slice(index);

    shift += prefix.length;
  }
  return output;
};

const jsonSchemaTypeToTs = (type: string, name: string, opType: string, items: any) => {
  switch (type) {
    case "string":
    case "date":
      return "string";

    case "integer":
      return "number";

    case "array":
      console.log("items", items);
      return `${jsonSchemaTypeToTs(items.type, name, opType, [])}[]`;

    case "object":
      return `${opType}${name[0].toUpperCase()}${name.slice(1)}`;

    default:
      break;
  }
};

program.parseAsync();

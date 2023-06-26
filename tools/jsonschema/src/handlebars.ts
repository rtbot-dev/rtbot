import Handlebars from "handlebars";
const fs = require("fs");
import { Runfiles } from "@bazel/runfiles";

const runfiles = new Runfiles(process.env);

export const compileTemplate = (template: string, options?: CompileOptions) =>
  Handlebars.compile(
    fs.readFileSync(runfiles.resolveWorkspaceRelative(`tools/jsonschema/templates/${template}.hbs`)).toString(),
    options
  );

export const registerPartial = (template: string) =>
  Handlebars.registerPartial(
    template,
    fs.readFileSync(runfiles.resolveWorkspaceRelative(`tools/jsonschema/templates/${template}.hbs`)).toString()
  );

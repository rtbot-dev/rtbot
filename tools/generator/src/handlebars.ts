import Handlebars from "handlebars";
const fs = require("fs");

export const compileTemplate = (template: string, options?: CompileOptions) =>
  Handlebars.compile(fs.readFileSync(__dirname + `/../templates/${template}.hbs`).toString(), options);

export const registerPartial = (template: string) =>
  Handlebars.registerPartial(template, fs.readFileSync(__dirname + `/../templates/${template}.hbs`).toString());

Handlebars.registerHelper("defined", function (s) {
  return !!s;
});


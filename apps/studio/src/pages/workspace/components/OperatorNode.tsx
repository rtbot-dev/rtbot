import React, { memo } from "react";
import { Handle, Position, Node } from "reactflow";
import Form from "@rjsf/core";
import zodToJsonSchema from "zod-to-json-schema";
import { formOperatorSchema } from "@/store/editor/operator.schemas";
import validator from "@rjsf/validator-ajv8";
import { withTheme, ThemeProps } from "@rjsf/core";
import { TitleFieldProps, WidgetProps } from "@rjsf/utils";
import "./form.css";

function OperatorTitleTemplate(props: TitleFieldProps) {
  //we don't want the title to be shown
  return <></>;
}
function OperatorBaseInputTemplate(props: WidgetProps) {
  return "";
}

const theme: ThemeProps = {
  templates: {
    TitleFieldTemplate: OperatorTitleTemplate,
    //BaseInputTemplate: OperatorBaseInputTemplate,
  },
};
const ThemedForm = withTheme(theme);

export type OperatorNodeInput = {
  data: { color: string; onChange: React.ChangeEventHandler<HTMLInputElement> };
  isConnectable: boolean;
};

let schema = zodToJsonSchema(formOperatorSchema, { $refStrategy: "none" });
console.log("JSON schemas", schema);
schema = {
  ...schema,
  // @ts-ignore
  anyOf: schema.anyOf.map((s) => ({
    ...s,
    title: s.properties.title["const"],
    properties: Object.entries(s.properties.parameters.properties)
      .map(([k, v]) => ({ [`${k}`]: (v as any).properties.value }))
      .reduce((acc, v) => ({ ...acc, ...v })),
    required: [],
  })),
};
export const OperatorNode = ({ data, isConnectable }: OperatorNodeInput) => {
  const onSubmit = (e) => {
    console.log("Submitting", e);
  };
  return (
    <>
      <Handle
        type="target"
        position={Position.Left}
        style={{ background: "#555" }}
        onConnect={(params) => console.log("handle onConnect", params)}
        isConnectable={isConnectable}
      />
      <div className="card bg-base-100 shadow-xl form-card">
        <ThemedForm className="card-body" schema={schema} validator={validator} onSubmit={onSubmit}>
          <button className="btn btn-primary">save</button>
        </ThemedForm>
      </div>
      <Handle
        type="source"
        position={Position.Right}
        id="b"
        style={{ bottom: 10, top: "auto", background: "#555" }}
        isConnectable={isConnectable}
      />
    </>
  );
};

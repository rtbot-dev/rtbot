import React, { memo, useState } from "react";
import { Handle, Position, Node } from "reactflow";
import Form from "@rjsf/core";
import zodToJsonSchema from "zod-to-json-schema";
import { formOperatorSchema } from "@/store/editor/operator.schemas";
import validator from "@rjsf/validator-ajv8";
import { withTheme, ThemeProps } from "@rjsf/core";
import { TitleFieldProps, WidgetProps } from "@rjsf/utils";
import "./form.css";
import { FaEdit, FaTrash } from "react-icons/all";
import { operatorSchemas } from "@/store/editor/operator.schemas";
import { NodeForm } from "./NodeForm";

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
    properties: {
      ...s.properties,
      title: {
        type: "string",
        default: s.properties.title["const"],
      },
    },
  })),
};

const uiSchema = ["title"].reduce(
  (acc, val) => ({
    ...acc,
    [`${val}`]: {
      "ui:widget": "hidden",
    },
  }),
  {}
);

type FormData = {
  title: string;
  [key: string]: any;
};

type State = {
  formData: FormData | null;
  formOpen: boolean;
  showMenu: boolean;
};
export const OperatorNode = ({
  data: {
    menu: { remove },
  },
  isConnectable,
}: OperatorNodeInput) => {
  const [state, setState] = useState<State>({
    formData: null,
    formOpen: false,
    showMenu: false,
  });
  const onSubmit = (e) => {
    // TODO: check if valid
    console.log("Submitting", e);
    setState({ ...state, formOpen: false, formData: e.formData });
  };

  const onFormChange = (e) => {
    console.log("Form changed", e);
  };

  return (
    <div>
      <Handle
        type="target"
        position={Position.Left}
        style={{ background: "#555" }}
        onConnect={(params) => console.log("handle onConnect", params)}
        isConnectable={isConnectable}
      />
      {state.formOpen ? (
        <div className="card bg-base-100 shadow-xl form-card" onClick={(e) => e.stopPropagation()}>
          <NodeForm schemas={operatorSchemas} />
          {/*<ThemedForm
            className="card-body"
            schema={schema}
            uiSchema={uiSchema}
            validator={validator}
            onSubmit={onSubmit}
            onChange={onFormChange}
          >
            <button className="btn btn-primary">save</button>
          </ThemedForm>*/}
        </div>
      ) : (
        <div
          style={{ position: "relative" }}
          onMouseEnter={() => setState({ ...state, showMenu: true })}
          onMouseLeave={() => setState({ ...state, showMenu: false })}
        >
          {state.showMenu && (
            <div className="btn-group node-menu">
              <button className="btn" onClick={() => setState({ ...state, formOpen: true })}>
                <FaEdit />
              </button>
              <button className="btn" onClick={remove}>
                <FaTrash />
              </button>
            </div>
          )}
          <div className="card bg-base-100 shadow-xl">
            <div className="card-body">
              {state.formData && (
                // show the form data
                <>
                  <strong>{state.formData.title}</strong>
                  {Object.keys(state.formData as any)
                    .filter((k) => k !== "title")
                    .map((k) => (
                      <div key={k}>
                        {k}={state.formData[k]}
                      </div>
                    ))}
                </>
              )}
            </div>
          </div>
        </div>
      )}
      <Handle
        type="source"
        position={Position.Right}
        id="b"
        style={{ bottom: 10, top: "auto", background: "#555" }}
        isConnectable={isConnectable}
      />
    </div>
  );
};

import { ZodEnum, ZodLiteral, ZodNumber, ZodObject, ZodString } from "zod";
import "./nodeform.css";
import { MdCheck, MdClose } from "react-icons/all";
import React, { useState } from "react";
import editor from "@/store/editor";

type NodeFormProps = {
  id: string;
  schemas: ZodObject<any>[];
};

type NodeFormState = {
  schema: ZodObject<any> | null;
  parameters: any;
  formErrors: any;
};
export const NodeForm = ({ schemas, id }: NodeFormProps) => {
  const [state, setState] = useState<NodeFormState>({ schema: null, parameters: {}, formErrors: {} });

  const setFormValue = (k: string, v: any) => {
    if (state.schema) {
      const { parameters } = state;
      parameters[k] = v;
      const { success, error } = state.schema.shape.parameters.safeParse(parameters);
      let formErrors = {};
      if (!success) {
        formErrors = Object.fromEntries(error.errors.map((e: any) => [`${e.path[0]}`, e.message]));
      }

      setState({ ...state, parameters: parameters, formErrors });
    }
  };

  return (
    <div className="form-control node-form">
      <select
        className="select select-bordered w-full node-form-main-selector"
        onChange={(event) =>
          setState({
            ...state,
            schema: schemas[event.target.options.selectedIndex - 1],
            parameters: {},
            formErrors: {},
          })
        }
      >
        <option disabled selected>
          Select operator
        </option>
        {schemas.map((s, k) => (
          <option key={k}>{s.shape.title.value}</option>
        ))}
      </select>
      {state.schema &&
        Object.entries(state.schema.shape.parameters.shape).map(([k, v], i) => {
          let inputComponent;
          switch (v.constructor) {
            case ZodNumber:
              inputComponent = (
                <input
                  type="number"
                  className="input input-bordered node-form-input-number"
                  onChange={(event) => setFormValue(k, parseFloat(event.target.value))}
                />
              );
              break;
            case ZodEnum:
              inputComponent = (
                <select className="select select-bordered" onChange={(event) => setFormValue(k, event.target.value)}>
                  <option disabled selected>
                    Select
                  </option>
                  {Object.entries(v.Values).map(([k, v], j) => (
                    <option key={j} value={k}>
                      {v}
                    </option>
                  ))}
                </select>
              );
              break;
            case ZodString:
              inputComponent = <input type="text" className="input input-bordered" />;
              break;
            case ZodLiteral:
              // do not display the title in the form
              return <></>;

            default:
              console.log("Unknown instance type for zod schema", v);
              return;
          }
          return (
            <div key={i}>
              <label className="input-group node-form-label">
                <span className="label-text">{k}</span>
                {inputComponent}
              </label>
              {state.formErrors[k] && <span className="label-text-alt">{state.formErrors[k]}</span>}
            </div>
          );
        })}

      <div className="node-form-buttons">
        <button className="btn btn-circle" onClick={() => editor.editOperator(id, false)}>
          <MdClose />
        </button>
        <button
          className="btn btn-circle"
          disabled={
            Object.keys(state.formErrors).length !== 0 ||
            Object.keys(state.parameters).length === 0 ||
            state.schema === null
          }
          onClick={() => {
            if (state.schema) {
              const opDef = {
                id,
                title: state.schema.shape.title.value,
                opType: state.schema.shape.opType.value,
                parameters: { ...state.parameters },
              };
              console.log("Saving", opDef);
              editor.updateOperator(opDef);
              editor.editOperator(id, false);
            }
          }}
        >
          <MdCheck />
        </button>
      </div>
    </div>
  );
};

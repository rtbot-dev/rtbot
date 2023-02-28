import { z, ZodEnum, ZodLiteral, ZodNumber, ZodObject, ZodSchema, ZodString, ZodTypeAny } from "zod";
import "./nodeform.css";
import { FaEdit, MdCheck, MdClose } from "react-icons/all";
import React, { useState } from "react";

type NodeFormPropsType = {
  schemas: ZodObject<any>[];
  schema: ZodObject<any> | null;
  formData: any;
  formErrors: any;
};
export const NodeForm = ({ schemas }: NodeFormPropsType) => {
  const [state, setState] = useState({ schema: null, schemas, formData: {}, formErrors: {} });

  const setFormValue = (k: string, v: any) => {
    const { formData } = state;
    formData[k] = v;
    const { success, error } = state.schema.omit({ title: true }).safeParse(formData);
    let formErrors = {};
    if (!success) {
      formErrors = Object.fromEntries(
        error.errors.map((e: { message: string; path: string[] }) => [e.path[0], e.message])
      );
    }

    setState({ ...state, formData, formErrors });
  };

  return (
    <div
      className="form-control node-form"
      onDrag={(event) => {
        console.log("Preventing event propagation");
        event.preventDefault();
        event.stopPropagation();
        event.nativeEvent.stopImmediatePropagation();
      }}
    >
      <select
        className="select select-bordered w-full node-form-main-selector"
        onChange={(event) =>
          setState({ ...state, schema: schemas[event.target.options.selectedIndex - 1], formData: {}, formErrors: {} })
        }
      >
        <option disabled selected>
          Select operator
        </option>
        {state.schemas.map((s, k) => (
          <option key={k}>{s.shape.title.value}</option>
        ))}
      </select>
      {state.schema &&
        Object.entries(state.schema.shape).map(([k, v], i) => {
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
        <button className="btn btn-circle" onClick={() => console.log("close edit")}>
          <MdClose />
        </button>
        <button
          className="btn btn-circle"
          disabled={Object.keys(state.formErrors).length !== 0 || Object.keys(state.formData).length === 0}
          onClick={() => {
            console.log("Saving");
          }}
        >
          <MdCheck />
        </button>
      </div>
    </div>
  );
};

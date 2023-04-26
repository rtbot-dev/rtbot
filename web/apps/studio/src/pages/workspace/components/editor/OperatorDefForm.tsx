import { ZodAny, ZodEnum, ZodLiteral, ZodNumber, ZodObject, ZodString } from "zod";
import "./nodeform.css";
import { MdCheck, MdClose } from "react-icons/all";
import React, { useLayoutEffect, useState } from "react";
import { CirclePicker } from "react-color";
import editor from "@/store/editor";
import { BaseOperator } from "@/store/editor/operator.schemas";
import menu from "@/store/menu";

type NodeFormProps = {
  id: string;
  programId: string;
  schemas: ZodObject<any>[];
  opDef: BaseOperator;
};

type NodeFormState = {
  schema: ZodObject<any> | null;
  parameters: any;
  formErrors: any;
  source?: string;
};
export const OperatorDefForm = ({ programId, schemas, id, opDef }: NodeFormProps) => {
  const schema = schemas.filter((s) => s.shape.opType.value === opDef.opType)[0];
  const [menuState, setMenuState] = useState(menu.getState());
  useLayoutEffect(() => {
    menu.subscribe(setMenuState);
  }, []);
  const [state, setState] = useState<NodeFormState>({
    schema,
    parameters: opDef.parameters ?? {},
    formErrors: {},
    source: opDef.metadata.source,
  });

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
          <option key={k} selected={s.shape.opType.value === opDef.opType}>
            {s.shape.title.value}
          </option>
        ))}
      </select>
      {state.schema &&
        Object.entries(state.schema.shape.parameters.shape).map(([k, v]: [string, any], i) => {
          let inputComponent;
          switch ((v as new () => ZodAny).constructor) {
            case ZodNumber:
              inputComponent = (
                <input
                  type="number"
                  className="input input-bordered node-form-input-number"
                  onChange={(event) => setFormValue(k, parseFloat(event.target.value))}
                  value={opDef.parameters ? opDef.parameters[k] : null}
                />
              );
              break;
            case ZodEnum:
              inputComponent = (
                <select className="select select-bordered" onChange={(event) => setFormValue(k, event.target.value)}>
                  <option disabled selected>
                    Select
                  </option>
                  {Object.entries(v.Values).map(([k1, v1]) => (
                    <option
                      key={`${k}-${k1}`}
                      value={v1 as string}
                      selected={opDef.parameters ? v1 === opDef.parameters[k] : false}
                    >
                      {v1 as string}
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
            <div key={`${opDef.id}-${i}`}>
              <label className="input-group node-form-label">
                <span className="label-text">{k}</span>
                {inputComponent}
              </label>
              {state.formErrors[k] && <span className="label-text-alt">{state.formErrors[k]}</span>}
            </div>
          );
        })}
      {state.schema && (
        <>
          {opDef.opType === "Input" && (
            <div>
              <label className="input-group node-form-label">
                <span className="label-text">source</span>
                <select
                  className="select select-bordered"
                  onChange={(event) => {
                    setState({ ...state, source: event.target.value });
                    editor.updateOperator(programId, { id, metadata: { source: event.target.value } });
                  }}
                >
                  <option disabled selected>
                    Select
                  </option>
                  {menuState.data.map((d, i) => (
                    <option
                      key={`${d.metadata.id}-${i}`}
                      value={d.metadata.id}
                      selected={d.metadata.id === state.source}
                    >
                      {d.title}
                    </option>
                  ))}
                </select>
              </label>
            </div>
          )}
        </>
      )}
      <div className="node-form-buttons">
        <button className="btn btn-circle" onClick={() => editor.editOperator(programId, id)}>
          <MdClose />
        </button>
        <div className="separator"></div>
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
              editor.updateOperator(programId, opDef);
              editor.editOperator(programId, id);
            }
          }}
        >
          <MdCheck />
        </button>
      </div>
    </div>
  );
};

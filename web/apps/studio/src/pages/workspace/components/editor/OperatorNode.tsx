import React, { useState } from "react";
import { Handle, Position } from "reactflow";
import "./form.css";
import { FaChartLine, FaEdit, FaTrash } from "react-icons/all";
import { operatorSchemaList } from "@/store/editor/operator.schemas";
import { NodeForm } from "./NodeForm";
import editor from "@/store/editor";
import { BaseOperator } from "@/store/editor/operator.schemas";
import { OperatorParameterValue } from "./OperatorParameterValue";

export type OperatorNodeInput = {
  id: string;
  data: BaseOperator & { programId: string };
  isConnectable: boolean;
};

type State = {
  showMenu: boolean;
  plot: boolean;
};
export const OperatorNode = ({ id, isConnectable, data }: OperatorNodeInput) => {
  const { parameters, title, metadata, programId } = data;
  const formOpen = metadata && typeof metadata.editing !== "undefined" ? metadata.editing : false;

  const [state, setState] = useState<State>({
    showMenu: false,
    plot: metadata && typeof metadata.plot !== "undefined" ? metadata.plot : false,
  });

  return (
    <div>
      <Handle
        type="target"
        position={Position.Left}
        style={{ background: "#555" }}
        onConnect={(params) => console.log("handle onConnect", params)}
        isConnectable={isConnectable}
      />
      {formOpen ? (
        <div className="card bg-base-100 shadow-xl form-card">
          <NodeForm programId={programId} schemas={operatorSchemaList} id={id} opDef={data} />
        </div>
      ) : (
        <div
          style={{ position: "relative" }}
          onMouseEnter={() => setState({ ...state, showMenu: true })}
          onMouseLeave={() => setState({ ...state, showMenu: false })}
        >
          {state.showMenu && (
            <div className="btn-group node-menu">
              <button className="btn" onClick={() => editor.editOperator(programId, id, true)}>
                <FaEdit />
              </button>
              <button
                className={`btn ${state.plot ? "btn-active" : ""}`}
                onClick={() => {
                  editor.updateOperator(programId, { id, metadata: { plot: !state.plot } });
                  setState({ ...state, plot: !state.plot });
                }}
              >
                <FaChartLine />
              </button>
              <button className="btn" onClick={() => editor.deleteOperator(programId, { id })}>
                <FaTrash />
              </button>
            </div>
          )}
          <div className="card bg-base-100 shadow-xl">
            <div className="card-body">
              {parameters && (
                // show the form data
                <>
                  <strong>{title}</strong>
                  {Object.keys(parameters).map((k) => (
                    <div key={k}>
                      {k}={parameters[k]}
                    </div>
                  ))}
                </>
              )}
              {metadata.style && (
                // show the style data
                <>
                  <strong>style</strong>
                  {Object.entries(metadata.style).map(([k, v], i) => (
                    <OperatorParameterValue parameter={k} value={v as string} key={i} />
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

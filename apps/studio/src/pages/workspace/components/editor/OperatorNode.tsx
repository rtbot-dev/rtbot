import React, { useState } from "react";
import { Handle, Position } from "reactflow";
import "./form.css";
import { FaEdit, FaTrash } from "react-icons/all";
import { operatorSchemaList } from "@/store/editor/operator.schemas";
import { NodeForm } from "./NodeForm";
import editor from "@/store/editor";
import { BaseOperator } from "@/store/editor/operator.schemas";

export type OperatorNodeInput = {
  id: string;
  data: BaseOperator;
  isConnectable: boolean;
};

type State = {
  showMenu: boolean;
};
export const OperatorNode = ({ id, isConnectable, data }: OperatorNodeInput) => {
  const { parameters, title, metadata } = data;
  const formOpen = metadata!! ? metadata.editing : false;

  const [state, setState] = useState<State>({
    showMenu: false,
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
          <NodeForm schemas={operatorSchemaList} id={id} opDef={data} />
        </div>
      ) : (
        <div
          style={{ position: "relative" }}
          onMouseEnter={() => setState({ ...state, showMenu: true })}
          onMouseLeave={() => setState({ ...state, showMenu: false })}
        >
          {state.showMenu && (
            <div className="btn-group node-menu">
              <button className="btn" onClick={() => editor.editOperator(id, true)}>
                <FaEdit />
              </button>
              <button className="btn" onClick={() => editor.deleteOperator({ id })}>
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
                  {Object.entries(metadata.style).map(([k, v]) => (
                    <div key={k}>
                      {k}={v}
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

import editor from "@/store/editor";
import React, { useState } from "react";
import { CirclePicker } from "react-color";
import { BaseOperator } from "../../../../store/editor/operator.schemas";
import { MdCheck, MdClose } from "react-icons/all";

export interface NodePlotFormProps {
  programId: string;
  id: string;
  opDef: BaseOperator;
}

export const OperatorPlotForm = ({ programId, id, opDef }: NodePlotFormProps) => {
  const color = opDef.metadata?.style?.color ?? "rgb(233, 30, 99)";
  const [state, setState] = useState({
    pickColor: false,
    newColor: color,
    addToPlot: opDef.metadata!.plot,
    mode: opDef.metadata!.style!.mode,
    legend: opDef.metadata!.style!.legend,
    stack: opDef.metadata!.style!.stack,
  });
  return (
    <div className="form-control node-form">
      <label className="input-group node-form-label">
        <span className="label-text">add to plot</span>
        <input
          type="checkbox"
          className="toggle input input-bordered"
          checked={state.addToPlot}
          onChange={(e) => setState({ ...state, addToPlot: e.target.checked })}
        />
      </label>
      {state.addToPlot && (
        <>
          <label className="input-group node-form-label">
            <span className="label-text">color</span>
            <span
              className="select select-bordered"
              onClick={() => setState({ ...state, pickColor: !state.pickColor })}
            >
              <div className="color-selected flex justify-center align-middle">
                <div
                  className="color-selected-inner-div"
                  style={{ boxShadow: `${state.newColor} 0px 0px 0px 15px inset` }}
                ></div>
              </div>
            </span>
          </label>
          <label className="input-group node-form-label">
            <span className="label-text">line type</span>
            <select
              className="select select-bordered"
              onChange={(event) => setState({ ...state, mode: event.target.value as any })}
            >
              <option disabled selected>
                Select
              </option>
              {Object.entries({
                lines: "lines",
                markers: "markers",
                text: "text",
                "lines and markers": "lines+markers",
                "text and markers": "text+markers",
                "text and lines": "text+lines",
                "text, lines and markers": "text+lines+markers",
              }).map(([k, v]) => (
                <option key={`${k}`} value={v as string} selected={v === state.mode}>
                  {v as string}
                </option>
              ))}
            </select>
          </label>
          <label className="input-group node-form-label">
            <span className="label-text">legend</span>
            <input
              type="text"
              className="input input-bordered"
              onChange={(e) => setState({ ...state, legend: e.target.value })}
              defaultValue={opDef.metadata?.style?.legend}
            />
          </label>
          <label className="input-group node-form-label">
            <span className="label-text">stack</span>
            <select
              className="select select-bordered"
              onChange={(event) => setState({ ...state, stack: parseInt(event.target.value) })}
            >
              {Array(5)
                .fill(1)
                .map((_, i) => (
                  <option key={`stack-${i}`} value={i + 1} selected={i + 1 === state.stack}>
                    {i === 0 ? "default" : i + 1}
                  </option>
                ))}
            </select>
          </label>
        </>
      )}
      {state.pickColor && (
        <CirclePicker
          onChange={(e: any) => {
            const colorSelected = `rgb(${e.rgb.r}, ${e.rgb.g}, ${e.rgb.b})`;
            console.log("Color selected", colorSelected);
            setState({ ...state, newColor: colorSelected });
          }}
        />
      )}
      <div className="node-form-buttons">
        <button className="btn btn-circle" onClick={() => editor.editOperator(programId, id)}>
          <MdClose />
        </button>
        <div className="separator"></div>
        <button
          className="btn btn-circle"
          onClick={() => {
            const opDef = {
              id,
              metadata: {
                style: {
                  color: state.newColor,
                  stack: state.stack,
                  ...(state.legend ? { legend: state.legend } : {}),
                  ...(state.mode ? { mode: state.mode } : {}),
                },
                plot: state.addToPlot,
              },
            };
            editor.updateOperator(programId, opDef);
            editor.editOperator(programId, id);
          }}
        >
          <MdCheck />
        </button>
      </div>
    </div>
  );
};

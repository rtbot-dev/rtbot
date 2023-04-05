import { IoPlay } from "react-icons/all";
import "./run-btn.css";
import { useLayoutEffect, useState } from "react";
import editor from "@/store/editor";
import { BaseOperator } from "@/store/editor/operator.schemas";
import { Program } from "@/store/editor/schemas";
import plot from "@/store/plot";
export const RunBtn = () => {
  const [editorState, setEditorState] = useState(editor.getState());
  useLayoutEffect(() => {
    editor.subscribe(setEditorState);
  }, []);

  let input: BaseOperator | null = null;
  if (editorState.program) {
    const inputs = editorState.program.operators.filter((op) => op.opType === "Input");
    if (inputs.length > 0) {
      if (inputs.length > 1) {
        console.log("More than 1 input found, we currently support max 1, using the first one found");
      }
      if (inputs[0].metadata.source) {
        input = inputs[0];
      }
    }
  }

  const disabled = input === null;

  return (
    <button
      className="run-btn btn btn-circle btn-outline"
      onClick={() => plot.run(editorState.program as Program, (input as BaseOperator).metadata.source as string)}
      disabled={disabled}
    >
      <IoPlay />
    </button>
  );
};

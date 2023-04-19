import { IoPlay } from "react-icons/all";
import "./run-btn.css";
import { useEffect, useLayoutEffect, useRef, useState } from "react";
import editor from "@/store/editor";
import { BaseOperator } from "@/store/editor/operator.schemas";
import { Program } from "@/store/editor/schemas";
import plot from "@/store/plot";
import { SinglePlotState } from "../../../../store/plot";

export type RunBtnProps = {
  programId: string;
};
export const RunBtn = ({ programId }: RunBtnProps) => {
  const [state, setState] = useState<{
    program?: Program;
    plot?: SinglePlotState;
    disabled?: boolean;
    source?: string;
    computingSec: number;
  }>({
    program: editor.getState().programs.find((p) => p.metadata?.id === programId),
    plot: plot.getState().plots.find((p) => p.id === programId),
    disabled: false,
    computingSec: 0,
  });
  const timerHandler = useRef<NodeJS.Timer | null>(null);

  useLayoutEffect(() => {
    editor.subscribe((editorState) => {
      let input: BaseOperator | null = null;
      const program = editorState.programs.find((p) => p.metadata?.id === programId);
      if (program) {
        const inputs = program.operators.filter((op) => op.opType === "Input");
        if (inputs.length > 0) {
          if (inputs.length > 1) {
            console.log("More than 1 input found, we currently support max 1, using the first one found");
          }
          if (inputs[0].metadata.source) {
            input = inputs[0];
          }
        }
      }

      const disabled = input === null || program?.metadata?.computing;

      setState({
        ...state,
        program,
        source: input?.metadata?.source,
        disabled,
      });
    });
    plot.subscribe((plotState) =>
      setState({
        ...state,
        plot: plotState.plots.find((p) => p.id === programId),
      })
    );
  }, []);

  const [counterState, setCounterState] = useState(0);

  useEffect(() => {
    if (state.program?.metadata?.computing && timerHandler.current === null) {
      console.log("Setting timer");
      timerHandler.current = setInterval(() => {
        setCounterState((c) => c + 1);
      }, 1000);
    }

    if (!state.program?.metadata?.computing && timerHandler.current !== null) {
      console.log("Clearing timer");
      clearTimeout(timerHandler.current);
      timerHandler.current = null;
      setCounterState(0);
    }
  });

  return state.disabled ? (
    <span className="countdown font-mono text-sm" style={{ marginTop: "20px", marginLeft: "20px" }}>
      <span style={{ "--value": `${Math.floor(counterState / 60)}` }}></span>m
      <span style={{ "--value": `${counterState % 60}` }}></span>s
    </span>
  ) : (
    <button
      className="run-btn btn btn-circle btn-outline"
      onClick={() => {
        editor.run(programId, state.source as string);
      }}
      disabled={state.disabled}
    >
      <IoPlay />
    </button>
  );
};

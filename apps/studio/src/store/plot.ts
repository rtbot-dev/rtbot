import { Subject } from "rxjs";
import { Figure } from "react-plotly.js";
import * as Plotly from "plotly.js";
import { Program } from "./editor/schemas";
import { rtbotApi } from "@/api/rtbot/rtbot.api";

const subject = new Subject<IPlotState>();

export interface IPlotState extends Figure {
  computing: boolean;
}

export const initialState: IPlotState = {
  computing: false,
  data: [],
  frames: null,
  layout: {},
};

let state = initialState;

// store
export const store = {
  init: () => {
    console.debug("Initializing plot store");
    state = { ...initialState };
    subject.next(state);
  },
  subscribe: (setState: (value: IPlotState) => void) => subject.subscribe(setState),
  run(program: Program, dataId: string) {
    console.log("Running program");
    state = { ...state, computing: true };
    subject.next(state);
    rtbotApi
      .run(program, dataId)
      .then((outputs) => {
        console.log("outputs", outputs);
        const data: Plotly.Data[] = [];
        Object.entries(outputs).map(([operatorId, output]) => {
          // TODO: handle case of larger vector output
          console.log("Adding output for", operatorId, output);
          const op = program.operators.find((o) => o.id === operatorId);
          data.push({
            x: output[0],
            y: output[1],
            type: "scattergl",
            mode: "lines+markers",
            marker: {
              size: 2,
              color: op.metadata.style.color,
            },
            line: {
              color: op.metadata.style.color,
              width: op.metadata.style.lineWidth ?? 1,
            },
          });
        });
        state = {
          computing: false,
          data,
          frames: null,
          layout: { title: `${program.metadata ? program.metadata.title : "program"} output` },
        };
        console.log("plot state", state);
        subject.next(state);
      })
      .catch((e) => console.error("Something went wrong while running program", program, "with data", dataId, e));
  },
  getState: () => ({ ...state }),
};

store.init();

export default store;

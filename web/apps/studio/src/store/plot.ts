import { Subject } from "rxjs";
import { Figure } from "react-plotly.js";
import * as Plotly from "plotly.js";
import { PlotStyle } from "./editor/schemas";

const subject = new Subject<IPlotState>();

export interface SinglePlotState extends Figure {
  id: string;
  computing: boolean;
}

export interface IPlotState {
  plots: SinglePlotState[];
}

export const initialState: IPlotState = {
  plots: [],
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
  setPlotComputing(plotId: string, computing: boolean) {
    const plots = state.plots.reduce(
      (plots: SinglePlotState[], p: SinglePlotState) => [
        ...plots,
        p.id === plotId
          ? {
              ...p,
              computing,
            }
          : p,
      ],
      []
    );

    state = { ...state, plots };
    subject.next({ ...state });
  },
  removePlot(plotId: string) {
    const plots = state.plots.filter((p) => p.id !== plotId);

    state = { ...state, plots };
    subject.next({ ...state });
  },
  upsertPlot(plotId: string, outputs: { [operatorId: string]: number[][] }, title: string, style: PlotStyle) {
    const plot = state.plots.find((p) => p.id === plotId);
    if (!plot) {
      // add a new plot to the list
      state.plots = [...state.plots, { id: plotId, data: [], frames: null, layout: {}, computing: false }];
    }
    const data: Plotly.Data[] = [];
    Object.entries(outputs).map(([operatorId, output]) => {
      // TODO: handle case of larger vector output
      console.log("Adding output for", operatorId, output);
      data.push({
        x: output[0],
        y: output[1],
        type: style.type,
        mode: "lines+markers",
        marker: {
          size: 2,
          color: style.color,
        },
        line: {
          color: style.color,
          width: style.lineWidth ?? 1,
        },
      });
    });
    // now update the plot in the list
    const plots = state.plots.reduce(
      (plots: SinglePlotState[], p: SinglePlotState) => [
        ...plots,
        p.id === plotId
          ? {
              ...p,
              computing: false,
              data,
              frames: null,
              layout: { title },
            }
          : p,
      ],
      []
    );

    state = { ...state, plots };
    subject.next({ ...state });
  },
  getState: () => ({ ...state }),
};

store.init();

export default store;

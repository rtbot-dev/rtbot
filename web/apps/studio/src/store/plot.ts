import { Subject } from "rxjs";
import { Figure } from "react-plotly.js";
import * as Plotly from "plotly.js";
import { PlotStyle } from "./editor/schemas";
import { BaseOperator } from "./editor/operator.schemas";
import { Dash, PlotData } from "plotly.js";
import { string } from "zod";

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
  upsertPlot(plotId: string, outputs: { [operatorId: string]: number[][] }, title: string, opDefs: BaseOperator[]) {
    const plot = state.plots.find((p) => p.id === plotId);
    if (!plot) {
      // add a new plot to the list
      state.plots = [...state.plots, { id: plotId, data: [], frames: null, layout: {}, computing: false }];
    }
    const data: Plotly.Data[] = [];
    const stacks: { [stack: string]: number } = {};
    Object.entries(outputs).map(([operatorId, output]) => {
      // get the style for the operator id
      const operator = opDefs.find((op) => op.id === operatorId)!;
      if (operator.metadata.plot) {
        const style = operator.metadata.style!;
        // by now we assign the same weight to all stacks
        // we can change later this in order to make some charts
        // to look larger inside the stack
        stacks[`${style.stack}`] = 1;

        for (let i = 1; i < output.length; i++) {
          console.log("Adding output ", i, " for", operatorId, output);
          let legend = style.legend ?? operatorId;
          legend = i === 1 ? legend : `${legend} ${i}`;
          // push the data
          data.push({
            x: output[0],
            y: output[i],
            name: legend,
            title: { text: legend },
            type: "scattergl",
            mode: style.mode as PlotData["mode"],
            marker: {
              size: 6,
              color: style.color,
            },
            line: {
              color: style.color,
              width: style.lineWidth ?? 1,
              dash: ["solid", "dot", "dash", "longdash", "dashdot", "longdashdot"][(i - 1) % 6] as Dash,
            },
            ...(style.stack > 1 ? { yaxis: `y${style.stack}` } : {}),
          });
        }
      }
    });
    // now update the plot in the list
    const totalStacks = Object.keys(stacks).length;
    // notice that the Object.keys output comes ordered
    const cursor = 0;
    const yAxisDomains = Object.keys(stacks).reduce(
      (acc, v) => ({
        ...acc,
        [`yaxis${v === "1" ? "" : v}`]: {
          domain: [Object.keys(acc).length / totalStacks, (Object.keys(acc).length + 1) / totalStacks],
        },
      }),
      {}
    );
    const plots = state.plots.reduce(
      (plots: SinglePlotState[], p: SinglePlotState) => [
        ...plots,
        p.id === plotId
          ? {
              ...p,
              computing: false,
              data,
              frames: null,
              layout: {
                title,
                ...yAxisDomains,
              },
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

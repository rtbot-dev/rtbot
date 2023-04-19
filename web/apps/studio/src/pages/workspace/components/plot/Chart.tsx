import Plot from "react-plotly.js";
import React, { useLayoutEffect, useRef, useState } from "react";
import plot from "@/store/plot";
import Lottie from "lottie-react";
import computingAnimation from "./calculator.json";
import { SinglePlotState } from "@/store/plot";

export type ChartProps = {
  plotId: string;
};
export function Chart({ plotId }: ChartProps) {
  const ref = useRef<any>(null);
  const [width, setWidth] = useState(0);
  const [height, setHeight] = useState(0);
  const [state, setState] = useState<SinglePlotState | undefined>(plot.getState().plots.find((p) => p.id === plotId));

  useLayoutEffect(() => {
    plot.subscribe((plotState) => {
      const plot = plotState.plots.find((p) => p.id === plotId);
      setState(plot);
    });
    if (ref.current !== null) {
      const { width, height } = ref.current.getBoundingClientRect();
      console.log("ref dimensions", width, height);
      setWidth(width);
      setHeight(height);
    }
  });
  return (
    <div ref={ref} style={{ height: "inherit", width: "inherit" }}>
      {state && state.data.length > 0 && !state.computing ? (
        <Plot
          data={state.data}
          layout={{
            ...state.layout,
            plot_bgcolor: "#1e293b",
            paper_bgcolor: "#1e293b",
            width,
            height,
            xaxis: { color: "#a6adbb" },
            yaxis: { color: "#a6adbb" },
          }}
          config={{ autosizable: true }}
        />
      ) : state?.computing ? (
        <Lottie animationData={computingAnimation} style={{ height: "50%" }} />
      ) : (
        <div>Nothing to show</div>
      )}
    </div>
  );
}

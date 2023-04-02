import Plot from "react-plotly.js";
import React, { useLayoutEffect, useRef, useState } from "react";
import plot from "@/store/plot";

export function Chart() {
  const ref = useRef<any>(null);
  const [width, setWidth] = useState(0);
  const [height, setHeight] = useState(0);
  const [plotState, setPlotState] = useState(plot.getState);
  useLayoutEffect(() => {
    plot.subscribe(setPlotState);
    if (ref.current !== null) {
      const { width, height } = ref.current.getBoundingClientRect();
      console.log("ref dimensions", width, height);
      setWidth(width);
      setHeight(height);
    }
  });
  return (
    <div ref={ref} style={{ height: "inherit", width: "inherit" }}>
      {plotState.data.length > 0 ? (
        <Plot
          data={plotState.data}
          layout={{
            ...plotState.layout,
            plot_bgcolor: "#1e293b",
            paper_bgcolor: "#1e293b",
            width,
            height,
            xaxis: { color: "#a6adbb" },
            yaxis: { color: "#a6adbb" },
          }}
          config={{ autosizable: true }}
        />
      ) : (
        <div>nothing to plot</div>
      )}
    </div>
  );
}

import * as vega from "vega-lite";
export const getVegaSpec = (width, height, plotPadding): vega.TopLevelSpec => ({
  $schema: "https://vega.github.io/schema/vega-lite/v5.json",
  data: { name: "table" },
  width: width - 2 * plotPadding,
  height: height - 2 * plotPadding,
  padding: plotPadding,
  bounds: "flush",
  autosize: {
    type: "fit",
  },
  config: {
    axis: {
      grid: true,
      gridOpacity: 0.1,
      tickOpacity: 0.1,
    },
    view: {
      stroke: "transparent",
      strokeWidth: 0,
      strokeOpacity: 0,
    },
  },
  layer: [
    {
      mark: {
        type: "line",
      },
      encoding: {
        x: {
          field: "x",
          type: "temporal",
          axis: {
            tickCount: 4,
            labelExpr: "[timeFormat(datum.value, '%H:%M:%S')]",
          },
        },
        y: {
          field: "y",
          type: "quantitative",
          title: "signal",
          axis: {
            tickCount: 4,
          },
        },
        color: { field: "output", type: "nominal", title: "op" },
      },
    },
    {
      mark: {
        type: "circle",
        filled: true,
        color: "#ffff00",
        opacity: 1,
      },
      encoding: {
        x: { field: "x", type: "temporal", title: "t" },
        y: {
          field: "peak",
          type: "quantitative",
          title: "peaks",
        },
        color: { field: "output", type: "nominal" },
        size: { value: 200 },
      },
    },
  ],
});

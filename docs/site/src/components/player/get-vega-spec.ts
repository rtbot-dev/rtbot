import * as vega from "vega-lite";
export const getVegaSpec = (
  width: number,
  height: number,
  plotPadding: number,
  opIds: string[],
  opColors: string[]
): vega.TopLevelSpec => ({
  $schema: "https://vega.github.io/schema/vega-lite/v5.json",
  data: { name: "table" },
  width: width - 2 * plotPadding,
  height: height - 2 * plotPadding,
  padding: plotPadding,
  autosize: { type: "fit" },
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
  // these will be updated through the js api
  params: [
    {
      name: "tmin",
      value: 0,
    },
    {
      name: "tmax",
      value: 10000,
    },
  ],
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
          scale: {
            domain: [{ expr: "tmin" }, { expr: "tmax" }],
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
        color: {
          field: "output",
          type: "nominal",
          title: "op",
          scale: {
            domain: opIds,
            range: opColors,
          },
        },
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

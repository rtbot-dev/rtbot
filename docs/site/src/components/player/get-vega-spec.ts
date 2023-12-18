import * as vega from "vega";
import { Program } from "@rtbot-dev/rtbot";
import { SET3_COLOR_SCHEME } from "./color-schemes";

const colorScheme = SET3_COLOR_SCHEME;

const computeLayers = (program: Program) =>
  // TODO:: we should show the output of all ports not only the o1 one
  program.operators.map((op, i) => ({
    transform: [
      { filter: { field: `${op.id}o1`, valid: true } },
      { filter: "datum.t > tmin" },
    ],
    mark: {
      type: "line",
      interpolate: "linear",
      color:
        op.metadata?.plot?.mark?.color ?? colorScheme[i % colorScheme.length],
      // override this with anything that comes from the op's metadata
      ...op.metadata?.plot?.mark,
    },
    encoding: {
      x: {
        field: "t",
        type: "temporal",
        axis: {
          ticks: false,
          tickCount: 4,
          labelBound: true,
          labelExpr: "[timeFormat(datum.value, '%H:%M:%S')]",
        },
        scale: {
          domain: [{ expr: "tmin" }, { expr: "tmax" }],
        },
      },
      y: {
        field: `${op.id}o1`,
        type: "quantitative",
        title: "signal",
        axis: {
          tickCount: 4,
        },
      },
    },
  }));

export const getVegaSpec = (
  width: number,
  height: number,
  plotPadding: number,
  program: Program
): vega.TopLevelSpec => {
  const layers = computeLayers(program);

  const spec = {
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
    layer: layers,
  };

  console.log("Vega Lite Spect", spec);
  return spec;
};

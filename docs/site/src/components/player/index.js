import React, { useState, useRef, useLayoutEffect, useEffect } from "react";
import { Editor as MonacoEditor } from "@monaco-editor/react";
import { Program } from "@rtbot-dev/rtbot";
import { useDebounce } from "usehooks-ts";
import JSON5 from "json5";
import { createSyntheticSignal } from "./streams/synthetic";
import { Vega } from "react-vega";
import * as vega from "vega";

const sampleProgram = `
// RtBot tutorial
// follow the instructions in the comments carefully!

{
  "entryOperator": "in1",
  "operators": [
    { "id": "in1", "type": "Input" },
    { "id": "ma1", "type": "MovingAverage", "n": 2 },
    // 1- try changing the value of n from 2 to 3, up to 8
    // 2- uncomment the following and the correspondent line in the connections
    //{ "id": "peak1", "type": "PeakDetector", "n": 16 },
  ],
  "connections": [
    { "from": "in1", "to": "ma1" },
    //{ "from": "ma1", "to": "peak1" },
  ],
}`;

const t0 = new Date().getTime();

export const Player = () => {
  const programRef = useRef(Program.toInstance(JSON5.parse(sampleProgram)));

  // resize
  const plotRef = useRef(null);
  const [width, setWidth] = useState(0);
  const [height, setHeight] = useState(0);

  useEffect(() => {
    const element = plotRef?.current;
    if (!element) return;

    const observer = new ResizeObserver(() => {
      const newWidth = Math.floor(element.getBoundingClientRect().width);
      const newHeight = Math.floor(element.getBoundingClientRect().height);
      if (width !== newWidth) setWidth(newWidth);
      if (height !== newHeight) setHeight(newHeight);
    });

    observer.observe(element);
    return () => {
      observer.disconnect();
    };
  }, [setWidth, setHeight]);

  useEffect(() => {
    if (programRef.current)
      programRef.current.start().catch((e) => console.error(e));
  });

  useLayoutEffect(() => {
    const data$ = createSyntheticSignal(100, 0.0015, 100, 80);
    data$.subscribe((p) => {
      // send the data to the program
      if (programRef.current) {
        programRef.current
          .processMessage(p.time, p.value)
          .then((result) => {
            if (view.current) {
              const tuples = Object.entries(result).reduce(
                (a, [opId, out]) => [
                  ...a,
                  ...Object.entries(out).reduce(
                    (acc, [port, msgs]) => [
                      ...acc,
                      ...msgs.map((m) => ({
                        output: `${opId}:${port}`,
                        x: new Date(t0 + m.time),
                        [opId.startsWith("peak") ? "peak" : "y"]: m.value,
                      })),
                    ],
                    []
                  ),
                ],
                []
              );

              const changeSet = vega
                .changeset()
                .insert(tuples)
                .remove(function (t) {
                  return t.x < t0 + p.time - 10000;
                });
              view.current.change("table", changeSet).run();
            }
          })
          .catch((e) => console.error(e));
      }
    });
  }, []);

  const [content, setContent] = useState(sampleProgram);
  const debouncedContent = useDebounce(content, 500);

  useEffect(() => {
    // Triggers when "debouncedContent" changes
    try {
      const programObj = JSON5.parse(content);
      const newProgram = Program.toInstance(programObj);
      const { success, error } = newProgram.safeValidate();

      if (!success) {
        console.log("There are validation errors in the program", error);
        return;
      }
      newProgram
        .start()
        .then(() => {
          console.log("Updating program running");
          if (programRef.current) programRef.current.stop();
          programRef.current = newProgram;
        })
        .catch((e) => console.error(e));
    } catch (error) {
      console.error(error);
    }
  }, [debouncedContent]);

  const handleEditorWillMount = (monaco) => {
    monaco.languages.json.jsonDefaults.setDiagnosticsOptions({
      comments: "ignore",
      trailingCommas: "ignore",
    });
  };

  const view = useRef(null);
  const plotPadding = width < 500 ? 10 : 80;

  const spec = {
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
        strokeWidth: 0,
        strokeOpacity: 0,
      },
      view: {
        stroke: "transparent",
        strokeWidth: 0,
        strokeOpacity: 0,
      },
      style: {
        cell: {
          stroke: "transparent",
        },
      },
    },
    layer: [
      {
        mark: {
          type: "line",
          points: true,
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
          color: { field: "output", type: "nominal" },
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
  };
  return (
    <div className="w-full grid grid-cols-6 gap-4 px-4">
      <div ref={plotRef} className="col-start-1 col-span-3 bg-yellow h-[50vh]">
        {width && height && (
          <Vega
            spec={spec}
            // TODO: with canvas renderer some border appear in chrome
            renderer="svg"
            actions={false}
            theme="dark"
            onNewView={(v) => (view.current = v)}
          />
        )}
      </div>
      <div className="col-start-4 col-span-3 h-[50vh] bg-slate-300">
        <MonacoEditor
          width={width}
          height={height}
          defaultLanguage="json"
          defaultValue={sampleProgram}
          theme={"vs-dark"}
          beforeMount={handleEditorWillMount}
          options={{ minimap: { enabled: false } }}
          onChange={(text) => {
            if (!text) return;
            setContent(text);
          }}
        />
      </div>
    </div>
  );
};

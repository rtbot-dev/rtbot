import React, { useState, useRef, useLayoutEffect, useEffect } from "react";
import { Editor as MonacoEditor } from "@monaco-editor/react";
import { Program } from "@rtbot-dev/rtbot";
import { useDebounce } from "usehooks-ts";
import JSON5 from "json5";
import { createSyntheticSignal } from "./streams/synthetic";
import BrowserOnly from '@docusaurus/BrowserOnly';
import ExecutionEnvironment from '@docusaurus/ExecutionEnvironment';

if (ExecutionEnvironment.canUseDOM) {
  const { default: Plot } = require("react-plotly.js");
  console.log("Plot", Plot, "loaded")
}

const sampleProgram = `
// RtBot tutorial
// follow the instructions in the comments carefully!

{
  "entryOperator": "in1",
  "operators": [
    { "id": "in1", "type": "Input" },
    { "id": "ma1", "type": "MovingAverage", "n": 2 },
    // 1- try changing the value of n from 2 to 3, up to 5
    // 2- uncomment the following and the correspondent line in the connections
    //{ "id": "peak1", "type": "PeakDetector", "n": 6 },
  ],
  "connections": [
    { "from": "in1", "to": "ma1" },
    //{ "from": "ma1", "to": "peak1" },
  ],
}`;
export const Player = () => {
  const programRef = useRef(Program.toInstance(JSON5.parse(sampleProgram)));

  const [output, setOutput] = useState({});

  // needed to trigger redraw of plotly
  const [datarevision, setDatarevision] = useState(0);

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
    const data$ = createSyntheticSignal(100, 0.25, 100, 80);
    data$.subscribe((p) => {
      // send the data to the program
      if (programRef.current) {
        programRef.current
          .processMessage(p.time, p.value)
          .then((result) => {
            console.log("Result program", programRef.current.programId, result);
            let newOutput = Object.keys(result).reduce((acc, k) => {
              if (!acc[k]) acc[k] = { x: [], y: [] };
              // here we consider only operators with a single output
              acc[k].x = [...acc[k].x, ...result[k].o1.map((v) => v.time)];
              acc[k].y = [...acc[k].y, ...result[k].o1.map((v) => v.value)];
              // max number of points stored
              if (acc[k].x.length > 50) {
                acc[k].x.shift();
                acc[k].y.shift();
              }
              return acc;
            }, output);
            // remove too old entries
            const inputTimes = output[programRef.current.entryOperator].x;
            const oldestInputTime = inputTimes[0];
            newOutput = Object.fromEntries(
              Object.entries(newOutput).map(([k, out]) => {
                for (let i = 0; i < out.x.length; i++) {
                  if (out.x[i] < oldestInputTime) {
                    out.x.shift();
                    out.y.shift();
                    i--;
                  }
                }
                return [k, out];
              })
            );
            setOutput(newOutput);
            setDatarevision(p.time);
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

  const [ignoreOutputs, setIgnoreOutputs] = useState([]);

  const traces = Object.entries(output).map(([k, { x, y }]) => ({
    name: k,
    x: x.map((t) => new Date(t * 1000)),
    y,
    type: "scattergl",
    mode: k.indexOf("peak") > -1 ? "markers" : "lines+markers",
    marker: {
      size: k.indexOf("peak") > -1 ? 10 : 3,
    },
    visible: ignoreOutputs.indexOf(k) < 0 ? true : "legendonly",
  }));

  return (
    <div className="w-full grid grid-cols-6 gap-4">
      <div ref={plotRef} className="col-start-1 col-span-3 bg-yellow h-[50vh]">
        <BrowserOnly>
        {() => {
            const { default: Plot } = require("react-plotly.js");
            return <Plot
              data={traces}
              layout={{
                plot_bgcolor: "#1e293b",
                paper_bgcolor: "#1e293b",
                title: "sine wave plus white noise",
                width,
                height,
                xaxis: { color: "#a6adbb" },
                yaxis: { color: "#a6adbb" },
                datarevision,
              }}
              onLegendClick={(event) => {
                console.log("trace selected", event);
                const { name, visible } = event.data[event.curveNumber];
                // toggle visible
                if (visible === "legendonly")
                  setIgnoreOutputs(ignoreOutputs.filter((k) => k !== name));
                else if (visible && ignoreOutputs.indexOf(name) < 0)
                  setIgnoreOutputs([...ignoreOutputs, name]);
              }}
            />
          }
        }
        </BrowserOnly>
      </div>
      <div className="col-start-4 col-span-3 h-[50vh] bg-slate-300">
        <MonacoEditor
          width={width}
          height={height}
          defaultLanguage="json5"
          defaultValue={sampleProgram}
          theme={"vs-dark"}
          onChange={(text) => {
            if (!text) return;
            setContent(text);
          }}
        />
      </div>
    </div>
  );
};

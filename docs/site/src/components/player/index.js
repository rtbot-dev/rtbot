import React, { useState, useRef, useLayoutEffect, useEffect } from "react";
import { Subject } from "rxjs";
import { Editor as MonacoEditor } from "@monaco-editor/react";
import { Program } from "@rtbot-dev/rtbot";
import Plot from "react-plotly.js";
import { useDebounce } from "usehooks-ts";
import JSON5 from "json5";

const ws$ = new Subject();
const ws = new WebSocket(
  // we will streaming the live market data for the pair btc/usdt
  // feel free to change this direction to stream whatever you want!
  "wss://stream.binance.com:443/ws/ethusdt@kline_1s"
);
console.log("ws", ws);

ws.onerror = (error) =>
  console.error(`Unable to open websocket connection`, error);

ws.onmessage = (event) => {
  // decode the message
  const msg = JSON.parse(event.data);
  // emit only the price property
  ws$.next({
    time: Math.round(msg.k.T / 1000),
    value: parseFloat(msg.k.c),
  });
};

const sampleProgram2 = `{
  // this is a sample program in rtbot
  // it has two operators connected 
  "entryOperator": "in1",
  "operators": [
    // each operator is identified with an "id" and has a "type"
    { "id": "in1", "type": "Input" },
    // some operators have extra parameters, like here n=3
    // try changing the value of n!
    { "id": "ma1", "type": "MovingAverage", "n": 3 },
    { 
      "id": "out1", 
      "type": "Output", 
      "metadata": { 
        "plot": {
          "legendPorts": {
            "o1": "ma(3)"
          }
        } 
      } 
    },
  ],
  "connections": [{ "from": "in1", "to": "ma1" }],
}`;

const sampleProgram = `{
  "entryOperator": "in1",
  "operators": [
    { "id": "in1", "type": "Input" },
    { "id": "ma1", "type": "MovingAverage", "n": 2 },
    // try changing the value of n to 3, 4, 10, ...!
  ],
  "connections": [{ "from": "in1", "to": "ma1" }],
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
    ws$.subscribe((p) => {
      // send the data to the program
      if (programRef.current) {
        programRef.current
          .processMessage(p.time, p.value)
          .then((result) => {
            console.log("Result", result);
            const newOutput = Object.keys(result).reduce((acc, k) => {
              if (!acc[k]) acc[k] = { x: [], y: [] };
              // here we consider only operators with a single output
              acc[k].x = [...acc[k].x, ...result[k].o1.map((v) => v.time)];
              acc[k].y = [...acc[k].y, ...result[k].o1.map((v) => v.value)];
              // max number of points stored
              if (acc[k].x.length > 80) {
                acc[k].x.shift();
                acc[k].y.shift();
              }
              return acc;
            }, output);
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
    mode: "lines+markers",
    marker: {
      size: 3,
    },
    visible: ignoreOutputs.indexOf(k) < 0 ? true : "legendonly",
  }));

  return (
    <div className="w-full grid grid-cols-6 gap-4">
      <div ref={plotRef} className="col-start-1 col-span-3 bg-yellow h-[50vh]">
        <Plot
          data={traces}
          layout={{
            plot_bgcolor: "#1e293b",
            paper_bgcolor: "#1e293b",
            title: "eth/usdt",
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

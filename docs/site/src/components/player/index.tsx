import React, { useState, useRef, useLayoutEffect, useEffect } from "react";
import { Editor as MonacoEditor } from "@monaco-editor/react";
import { Program, Operator } from "@rtbot-dev/rtbot";
import { useDebounce } from "usehooks-ts";
import JSON5 from "json5";
import { Vega } from "react-vega";
import * as vega from "vega";
import { getVegaSpec } from "./vega-spec";
import { Subject } from "rxjs";
import { View } from "vega";
import { Monaco } from "@monaco-editor/react";

const t0 = new Date().getTime();

export type PlayerProps = {
  getStream: () => Subject<{ time: number; value: number }>;
  programStr: string;
};

export const Player = ({ getStream, programStr }: PlayerProps) => {
  const programRef = useRef(Program.toInstance(JSON5.parse(programStr)));

  // resize
  const plotRef = useRef<HTMLDivElement>(null);
  const [width, setWidth] = useState(0);
  const [height, setHeight] = useState(0);

  useEffect(() => {
    const element: HTMLDivElement | null = plotRef?.current;
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

  const opByViewIdsRef = useRef<{ [id: string]: string[] }>({});

  useLayoutEffect(() => {
    if (programRef.current) {
      programRef.current
        .start()
        .then(() => {
          const data$ = getStream(); //createSyntheticSignal(100, 0.0015, 100, 80);
          data$.subscribe((p) => {
            // send the data to the program
            if (programRef.current) {
              programRef.current
                .processMessage(p.time, p.value)
                .then((result) => {
                  // update each one of the views
                  Object.entries(opByViewIdsRef.current).forEach(
                    ([viewId, ops]) => {
                      const tuples = Object.entries(result)
                        .filter(([opId]) => {
                          return ops.indexOf(opId) > -1;
                        })
                        .reduce(
                          (a, [opId, out]) => [
                            ...a,
                            ...Object.entries(out).reduce(
                              (acc, [port, msgs]) => [
                                ...acc,
                                ...msgs.map((m) => ({
                                  output: `${opId}:${port}`.replace(":o1", ""),
                                  x: new Date(t0 + m.time),
                                  [opId.startsWith("peak") ? "peak" : "y"]:
                                    m.value,
                                })),
                              ],
                              [] as { [k: string]: string | number | Date }[]
                            ),
                          ],
                          [] as { [k: string]: string | number | Date }[]
                        );

                      if (viewsRef.current[viewId]) {
                        const changeSet = vega
                          .changeset()
                          .insert(tuples)
                          .remove(function (t: { x: number }) {
                            return t.x < t0 + p.time - 10000;
                          });
                        viewsRef.current[viewId]
                          .change("table", changeSet)
                          .resize()
                          .run();
                      } else {
                        console.warn("View", viewId, "not found, ignoring");
                      }
                    }
                  );
                })
                .catch((e) => console.error(e));
            }
          });
        })
        .catch((e) => console.error(e));
    }
  }, []);

  const [content, setContent] = useState(programStr);
  const debouncedContent = useDebounce(content, 500);

  const computeOpByViewIds = (program: Program) =>
    program.operators.reduce((acc, op: Operator) => {
      const plotId = op.metadata?.plot?.id ?? "default";
      if (acc[plotId]) acc[plotId].push(op.id);
      else acc[plotId] = [op.id];
      return { ...acc };
    }, {} as { [viewId: string]: string[] });

  const [, setForceRender] = useState<any>();
  useEffect(() => {
    // Triggers when "debouncedContent" changes
    try {
      const programObj = JSON5.parse(content);
      const newProgram = Program.toInstance(programObj);
      // @ts-ignore
      const { success, error } = newProgram.safeValidate();

      if (!success) {
        console.log("There are validation errors in the program", error);
        return;
      }
      // update the possible views
      opByViewIdsRef.current = computeOpByViewIds(newProgram);
      setForceRender(Object.entries(opByViewIdsRef.current));

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

  const handleEditorWillMount = (monaco: Monaco) => {
    monaco.languages.json.jsonDefaults.setDiagnosticsOptions({
      comments: "ignore",
      trailingCommas: "ignore",
    });
  };

  const viewsRef = useRef<{ [id: string]: View }>({});
  const plotPadding = width < 500 ? 10 : 80;
  const vegaSpec = getVegaSpec(width, height, plotPadding);

  return (
    <div className="w-full grid grid-cols-6 gap-4 px-4">
      <div ref={plotRef} className="col-start-1 col-span-3 bg-yellow h-[50vh]">
        {width &&
          height &&
          Object.keys(opByViewIdsRef.current).map((viewId) => (
            <Vega
              key={viewId}
              spec={vegaSpec}
              // TODO: with canvas renderer some border appears in chrome
              renderer="svg"
              actions={false}
              theme="dark"
              onNewView={(v) => {
                viewsRef.current[viewId] = v;
              }}
            />
          ))}
      </div>
      <div className="col-start-4 col-span-3 h-[50vh] bg-slate-300">
        <MonacoEditor
          width={width}
          height={height}
          defaultLanguage="json"
          defaultValue={programStr}
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

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

export type PlayerProps = {
  getStream: () => Subject<{ time: number; value: number }>;
  programStr: string;
  t0: number;
  tWindowSize: number;
};

export const Player = ({
  getStream,
  programStr,
  t0,
  tWindowSize,
}: PlayerProps) => {
  const [, setForceRender] = useState<any>();
  const programRef = useRef(Program.toInstance(JSON5.parse(programStr)));

  // resize
  const plotRef = useRef<HTMLDivElement>(null);
  const dimensionsRef = useRef<{
    width: number;
    height: number;
    plotPadding: number;
  }>({
    width: 0,
    height: 0,
    plotPadding: 0,
  });

  useEffect(() => {
    const element: HTMLDivElement | null = plotRef?.current;
    if (!element) return;

    const observer = new ResizeObserver(() => {
      const newWidth = Math.floor(element.getBoundingClientRect().width);
      const newHeight = Math.floor(element.getBoundingClientRect().height);
      const numViews = Object.keys(opByViewIdsRef.current).length;
      if (dimensionsRef.current.width !== newWidth) {
        dimensionsRef.current.width = newWidth;
        dimensionsRef.current.plotPadding =
          dimensionsRef.current.width < 500 ? 10 : 80;
        if (dimensionsRef.current.height / numViews < 500)
          dimensionsRef.current.plotPadding = 5;
        setForceRender(Object.entries(dimensionsRef.current));
      }
      if (dimensionsRef.current.height !== newHeight) {
        dimensionsRef.current.height = newHeight;
        setForceRender(Object.entries(dimensionsRef.current));
      }
    });

    observer.observe(element);
    return () => {
      observer.disconnect();
    };
  }, []);

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
                  const numViews = Object.keys(opByViewIdsRef.current).length;
                  console.log("numViews", numViews, dimensionsRef.current);

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
                            return t.x < t0 + p.time - tWindowSize;
                          });
                        viewsRef.current[viewId]
                          .change("table", changeSet)
                          .width(
                            dimensionsRef.current.width -
                              2 * dimensionsRef.current.plotPadding
                          )
                          .height(
                            dimensionsRef.current.height / numViews -
                              2 * dimensionsRef.current.plotPadding -
                              3
                          )
                          .padding(dimensionsRef.current.plotPadding)
                          .signal("tmax", t0 + p.time)
                          .signal("tmin", t0 + p.time - tWindowSize)
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
      if (acc[plotId] && acc[plotId].indexOf(op.id) < 0)
        acc[plotId].push(op.id);
      else acc[plotId] = [op.id];
      return { ...acc };
    }, {} as { [viewId: string]: string[] });

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
      console.log("new opByViewIds", computeOpByViewIds(newProgram));
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

  const vegaSpec = getVegaSpec(
    dimensionsRef.current.width,
    dimensionsRef.current.height,
    dimensionsRef.current.plotPadding
  );

  return (
    <div className="w-full grid grid-cols-6 gap-4 px-4">
      <div
        ref={plotRef}
        className="col-start-1 col-span-3 gap-0 bg-yellow h-[50vh]"
      >
        {Object.keys(opByViewIdsRef.current).map((viewId) => (
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
          width={dimensionsRef.current.width}
          height={dimensionsRef.current.height}
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

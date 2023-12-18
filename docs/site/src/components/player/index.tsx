import React, { useState, useRef, useLayoutEffect, useEffect } from "react";
import { Editor as MonacoEditor } from "@monaco-editor/react";
import { Program, Operator } from "@rtbot-dev/rtbot";
import { useDebounce } from "usehooks-ts";
import JSON5 from "json5";
import { Vega } from "react-vega";
import * as vega from "vega";
import { getVegaSpec } from "./get-vega-spec";
import { Subject } from "rxjs";
import { View } from "vega";
import { Monaco } from "@monaco-editor/react";
import * as R from "ramda";

export type PlayerProps = {
  getStream: () => Subject<{ time: number; value: number }>;
  programStr: string;
  t0: number;
  initialWindowSize: number;
};

type VegaRef = {
  dimensions: {
    width: number;
    height: number;
    plotPadding: number;
  };
  timeScale: {
    t: number;
    dt: number;
  };
  spec: any;
  opByViewIds: { [id: string]: string[] };
  views: { [id: string]: View };
  values: { [id: string]: any };
};

const computeOpByViewIds = (program: Program) =>
  program.operators.reduce((acc, op: Operator) => {
    const plotId = op.metadata?.plot?.id ?? "default";
    if (acc[plotId] && acc[plotId].indexOf(op.id) < 0) acc[plotId].push(op.id);
    else acc[plotId] = [op.id];
    return { ...acc };
  }, {} as { [viewId: string]: string[] });

export const Player = ({
  getStream,
  programStr,
  t0,
  initialWindowSize,
}: PlayerProps) => {
  const [, setForceRender] = useState<any>();
  const programRef = useRef(Program.toInstance(JSON5.parse(programStr)));

  // resize
  const plotRef = useRef<HTMLDivElement>(null);
  const configRef = useRef<VegaRef>({
    dimensions: {
      width: 0,
      height: 0,
      plotPadding: 0,
    },
    timeScale: {
      dt: initialWindowSize,
    },
    spec: undefined,
    opByViewIds: {},
    views: {},
    values: {},
  });

  useEffect(() => {
    const element: HTMLDivElement | null = plotRef?.current;
    if (!element) return;

    const observer = new ResizeObserver(() => {
      const newWidth = Math.floor(element.getBoundingClientRect().width);
      const newHeight = Math.floor(element.getBoundingClientRect().height);
      const numViews = Object.keys(configRef.current.opByViewIds).length;
      if (configRef.current.dimensions.width !== newWidth) {
        configRef.current.dimensions.width = newWidth;
      }
      if (configRef.current.dimensions.height !== newHeight) {
        configRef.current.dimensions.height = newHeight;
      }
      configRef.current.dimensions.plotPadding =
        configRef.current.dimensions.width < 300 ||
        configRef.current.dimensions.height / numViews < 300
          ? 10
          : 80;
      configRef.current.values = Object.entries(configRef.current.views).reduce(
        (acc, [viewId, view]) => ({ ...acc, [viewId]: view.data("data_0") }),
        {}
      );
      setForceRender(Object.entries(configRef.current.dimensions));
    });

    observer.observe(element);
    return () => {
      observer.disconnect();
    };
  }, []);

  useLayoutEffect(() => {
    if (programRef.current) {
      programRef.current
        .start()
        .then(() => {
          const data$ = getStream();
          data$.subscribe((p) => {
            // send the data to the program
            if (programRef.current) {
              programRef.current
                .processMessage(p.time, p.value)
                .then((result) => {
                  const numViews = Object.keys(
                    configRef.current.opByViewIds
                  ).length;
                  // update time scale range
                  const tmax = t0 + p.time;
                  const tmin = t0 + p.time - configRef.current.timeScale.dt;
                  configRef.current.timeScale.t = tmax;

                  // update each one of the views
                  Object.entries(configRef.current.opByViewIds).forEach(
                    ([viewId, ops]) => {
                      const tuplesPerOp = Object.entries(result)
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
                                  t: t0 + m.time,
                                  [`${opId}${port}`]: m.value,
                                })),
                              ],
                              [] as { [k: string]: string | number | Date }[]
                            ),
                          ],
                          [] as { [k: string]: string | number | Date }[]
                        );

                      const tuples = R.map(
                        R.mergeAll,
                        R.collectBy(R.prop("t"), tuplesPerOp)
                      );
                      //console.log("tuples", tuples);
                      if (configRef.current.views[viewId]) {
                        const changeSet = vega
                          .changeset()
                          .insert(tuples)
                          .remove(function ({ t }: { t: number }) {
                            // keep values up to the original window size
                            return t < t0 + p.time - initialWindowSize;
                          });
                        configRef.current.views[viewId]
                          .change("table", changeSet)
                          .width(
                            configRef.current.dimensions.width -
                              2 * configRef.current.dimensions.plotPadding
                          )
                          .height(
                            configRef.current.dimensions.height / numViews -
                              2 * configRef.current.dimensions.plotPadding -
                              3
                          )
                          .padding(configRef.current.dimensions.plotPadding)
                          .signal("tmax", tmax)
                          .signal("tmin", tmin)
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
      configRef.current.opByViewIds = computeOpByViewIds(newProgram);
      configRef.current.values = Object.entries(configRef.current.views).reduce(
        (acc, [viewId, view]) => ({ ...acc, [viewId]: view.data("data_0") }),
        {}
      );
      console.log("init values", configRef.current.values);
      configRef.current.spec = getVegaSpec(
        configRef.current.dimensions.width,
        configRef.current.dimensions.height,
        configRef.current.dimensions.plotPadding,
        newProgram
      );

      setForceRender(Object.entries(configRef.current.opByViewIds));

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

  // do not scroll page while using the wheel over the vega charts
  const preventDefault = (e: any) => {
    e = e || window.event;
    if (e.preventDefault) {
      e.preventDefault();
    }
    e.returnValue = false;
  };

  return (
    <div className="w-full grid grid-cols-6 gap-4 px-4">
      <div
        ref={plotRef}
        className="col-start-1 col-span-3 gap-0 bg-yellow h-[50vh]"
        onMouseLeave={() => {
          document.removeEventListener("wheel", preventDefault, false);
        }}
        onMouseEnter={() => {
          document.addEventListener("wheel", preventDefault, {
            passive: false,
          });
        }}
      >
        {Object.keys(configRef.current.opByViewIds).map((viewId) => (
          <Vega
            key={viewId}
            spec={configRef.current.spec}
            // TODO: with canvas renderer some border appears in chrome
            renderer="canvas"
            actions={false}
            theme="dark"
            onNewView={(v) => {
              const changeSet = vega
                .changeset()
                .insert(configRef.current.values[viewId] ?? []);
              // enable zoom
              v.addEventListener("wheel", (e) => {
                const dt = Math.min(
                  Math.max(
                    configRef.current.timeScale.dt *
                      (1 + 0.1 * Math.sign(e.deltaY)),
                    initialWindowSize / 10
                  ),
                  initialWindowSize
                );
                configRef.current.timeScale.dt = dt;
              });
              v.change("table", changeSet)
                .signal("tmax", configRef.current.timeScale.t)
                .signal(
                  "tmin",
                  configRef.current.timeScale.t - configRef.current.timeScale.dt
                )
                .run();
              configRef.current.views[viewId] = v;
            }}
          />
        ))}
      </div>
      <div className="col-start-4 col-span-3 h-[50vh] bg-slate-300">
        <MonacoEditor
          width={configRef.current.dimensions.width}
          height={configRef.current.dimensions.height}
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

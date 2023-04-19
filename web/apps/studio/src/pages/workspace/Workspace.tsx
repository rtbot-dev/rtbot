import { authenticated } from "@/pages/authenticated";
import { Data } from "./components/menu/Data";
import { NavBar } from "./components/top/NavBar";
import { WindowsManager, Window } from "@rtbot/ui-windows-manager";
import { Programs } from "./components/menu/Programs";
import { useLayoutEffect, useState } from "react";
import editor from "@/store/editor";
import { GraphEditor } from "./components/editor/GraphEditor";
import plot from "@/store/plot";
import { Chart } from "./components/plot/Chart";

export const Workspace = authenticated(() => {
  const [state, setState] = useState(editor.getState);
  const [plotState, setPlotState] = useState(plot.getState);
  useLayoutEffect(() => {
    editor.subscribe(setState);
    plot.subscribe(setPlotState);
  }, []);

  return (
    <div>
      <NavBar />
      {/* this is a dummy div to force tailwind to include the classes needed for the windows manager*/}
      <div
        className="resize overflow-auto text-sm p-2 ring-1 ring-slate-900/10 shadow-sm dark:bg-slate-800 dark:ring-0 dark:highlight-white/5 bg-slate-200 bg-base-200"
        style={{ visibility: "hidden" }}
      ></div>
      <WindowsManager>
        <Window
          id="programs"
          title="Programs"
          zIndex={0}
          initialStyle={{
            top: "80px",
            left: "20px",
            width: "400px",
            height: "400px",
          }}
          notClosable={true}
        >
          <Programs />
        </Window>
        <Window
          id="data"
          title="Data"
          zIndex={1}
          initialStyle={{
            top: "500px",
            left: "20px",
            width: "400px",
            height: "600px",
          }}
          notClosable={true}
        >
          <Data />
        </Window>
        {state.programs.map((p, i) => {
          return (
            <Window
              id={`w-${p.metadata?.id as string}`}
              title={p.metadata?.title ?? ""}
              key={`w-${i}`}
              zIndex={i + 2}
              initialStyle={{
                top: "200px",
                left: "400px",
                width: "900px",
                height: "700px",
              }}
              onClose={(windowId: string) => editor.closeProgram(windowId.split("-")[1])}
            >
              <GraphEditor programId={p.metadata?.id as string} />
            </Window>
          );
        })}
        {plotState.plots.map((p, i) => {
          return (
            <Window
              id={`p-${p.id}`}
              title={`Plot ${p.layout?.title as string}` ?? `Plot ${p.id}`}
              key={`p-${i}`}
              zIndex={state.programs.length + i + 2}
              initialStyle={{
                top: "100px",
                left: "600px",
                width: "1000px",
                height: "700px",
              }}
              onClose={(windowId: string) => plot.removePlot(windowId.split("-")[1])}
            >
              <Chart plotId={p.id} />
            </Window>
          );
        })}
      </WindowsManager>
    </div>
  );
});

import { signOut } from "@/api/firebase";
import { authenticated } from "@/pages/authenticated";
import { UserMenu } from "@/pages/workspace/components/editor/UserMenu";
import { ProgramEditor } from "./components/editor/ProgramEditor";
import { GraphEditor } from "./components/editor/GraphEditor";
import { SideMenu } from "./components/menu/SideMenu";
import { NavBar } from "./components/top/NavBar";
import { Chart } from "./components/plot/Chart";
import { Window } from "@rtbot/ui-windows-manager";

export const Workspace = authenticated(() => (
  <div>
    <NavBar />
    <SideMenu>
      <div className="w-full ">
        <div
          className="resize overflow-auto text-sm p-2 ring-1 ring-slate-900/10 shadow-sm dark:bg-slate-800 dark:ring-0 dark:highlight-white/5"
          style={{ height: "40vh", width: "99%", margin: "10px" }}
        >
          <Window>Nice</Window>
          <Chart />
        </div>
        <div
          className="resize overflow-auto text-sm p-2 ring-1 ring-slate-900/10 shadow-sm dark:bg-slate-800 dark:ring-0 dark:highlight-white/5"
          style={{ height: "45vh", width: "99%", margin: "10px" }}
        >
          <GraphEditor />
        </div>
      </div>
    </SideMenu>
  </div>
));

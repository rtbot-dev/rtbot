import { signOut } from "@/api/firebase";
import { authenticated } from "@/pages/authenticated";
import { UserMenu } from "@/pages/workspace/components/editor/UserMenu";
import { ProgramEditor } from "./components/editor/ProgramEditor";
import { GraphEditor } from "./components/editor/GraphEditor";
import { SideMenu } from "./components/menu/SideMenu";
import { NavBar } from "./components/top/NavBar";

export const Workspace = authenticated(() => (
  <div>
    <NavBar />
    <SideMenu>
      <div className="w-full flex justify-center">
        <div
          className="resize overflow-auto text-sm p-2 ring-1 ring-slate-900/10 shadow-sm dark:bg-slate-800 dark:ring-0 dark:highlight-white/5"
          style={{ height: "90vh", width: "100%", margin: "10px" }}
        >
          <GraphEditor />
        </div>
        <div
          className="resize overflow-auto text-sm p-2 ring-1 ring-slate-900/10 shadow-sm dark:bg-slate-800 dark:ring-0 dark:highlight-white/5"
          style={{ height: "90vh", width: "100vh", margin: "10px" }}
        >
          <ProgramEditor />
        </div>
      </div>
    </SideMenu>
  </div>
));

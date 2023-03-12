import React, { useLayoutEffect, useState } from "react";
import menu from "@/store/menu";
import { ProgramEntry } from "./ProgramEntry";
import { IoAddCircle } from "react-icons/all";
import { programFirestoreApi } from "../../../../api/program/program.firestore.api";

export const SideMenu = ({ children }: React.PropsWithoutRef<any>) => {
  const [state, setState] = useState(menu.getState());
  useLayoutEffect(() => {
    menu.subscribe(setState);
  }, []);

  return (
    <div className="drawer">
      <input id="workspace-drawer" type="checkbox" className="drawer-toggle" checked={state.sideMenuOpen} />
      <div className="drawer-content">{children}</div>
      <div className="drawer-side">
        <label htmlFor="workspace-drawer" className="drawer-overlay"></label>
        <div className="menu p-4 w-80 bg-base-100 text-base-content">
          <div>Projects</div>
          <ul>
            {state.programs.map((p, i) => (
              <li key={i} className="flex items-center">
                <ProgramEntry {...p} />
              </li>
            ))}
            <li>
              <button className="justify-center btn-xl" onClick={() => menu.createProgram()}>
                <IoAddCircle />
              </button>
            </li>
          </ul>
        </div>
      </div>
    </div>
  );
};

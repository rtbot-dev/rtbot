import React, { useLayoutEffect, useState } from "react";
import editor from "@/store/editor";
import menu from "@/store/menu";
import { ProgramEntry } from "./ProgramEntry";
import { IoAddCircle, IoCloudUpload } from "react-icons/all";
import "./menu.css";

export const SideMenu = ({ children }: React.PropsWithoutRef<any>) => {
  const [state, setState] = useState(menu.getState());
  const [editorState, setEditorState] = useState(editor.getState());
  useLayoutEffect(() => {
    menu.subscribe(setState);
    editor.subscribe(setEditorState);
  }, []);

  const opened = state.sideMenuOpen || editorState.program === null;

  return (
    <div className="drawer">
      <input id="workspace-drawer" type="checkbox" className="drawer-toggle" checked={opened} />
      <div className="drawer-content">{children}</div>
      <div className="drawer-side">
        <label htmlFor="workspace-drawer" className="drawer-overlay"></label>
        <div className="menu p-4 w-80 bg-base-100 text-base-content">
          <div>
            <strong>Programs</strong>
          </div>
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
          <div>
            <strong>Data</strong>
            <ul>
              <li>
                <button className="justify-center btn-xl">
                  <IoCloudUpload />
                </button>
              </li>
            </ul>
          </div>
        </div>
      </div>
    </div>
  );
};

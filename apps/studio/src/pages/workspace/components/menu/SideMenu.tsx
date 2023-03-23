import React, { useLayoutEffect, useState } from "react";
import editor from "@/store/editor";
import menu from "@/store/menu";
import { ProgramEntry } from "./ProgramEntry";
import { IoAddCircle, IoCloudUpload } from "react-icons/all";
import "./menu.css";
import { DataEntry } from "./DataEntry";

export const SideMenu = ({ children }: React.PropsWithoutRef<any>) => {
  const [selectedFile, setSelectedFile] = useState();
  const [state, setState] = useState(menu.getState());
  const [editorState, setEditorState] = useState(editor.getState());
  useLayoutEffect(() => {
    menu.subscribe(setState);
    editor.subscribe(setEditorState);
  }, []);

  const opened = state.sideMenuOpen || editorState.program === null;
  const fileHandler = (e: any) => {
    setSelectedFile(e.target.files[0]);
  };

  const uploadFile = () => {
    console.log("Uploading file", selectedFile);
    menu.uploadFile(selectedFile as unknown as File);
  };

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
              {state.editingProgramList ? (
                <div className="justify-center">...</div>
              ) : (
                <button className="justify-center btn-xl" onClick={() => menu.createProgram()}>
                  <IoAddCircle />
                </button>
              )}
            </li>
          </ul>
          <div className="divider"></div>
          <div>
            <strong>Data</strong>
            <ul>
              {state.data.map((p, i) => (
                <li key={i} className="flex items-center">
                  <DataEntry {...p} />
                </li>
              ))}
            </ul>
            <span>
              <input type="file" className="file-input w-60 max-w-xs" accept="text/csv" onChange={fileHandler} />
              {selectedFile && (
                <button className="justify-center btn-xl" onClick={uploadFile}>
                  <IoCloudUpload />
                </button>
              )}
            </span>
          </div>
        </div>
      </div>
    </div>
  );
};

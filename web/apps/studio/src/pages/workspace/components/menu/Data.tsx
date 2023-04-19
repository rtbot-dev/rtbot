import React, { useLayoutEffect, useState } from "react";
import editor from "@/store/editor";
import menu from "@/store/menu";
import { ProgramEntry } from "./ProgramEntry";
import { IoAddCircle, IoCloudUpload } from "react-icons/all";
import "./menu.css";
import { DataEntry } from "./DataEntry";

export const Data = () => {
  const [selectedFile, setSelectedFile] = useState();
  const [state, setState] = useState(menu.getState());
  useLayoutEffect(() => {
    menu.subscribe(setState);
  }, []);

  const fileHandler = (e: any) => {
    setSelectedFile(e.target.files[0]);
  };

  const uploadFile = () => {
    console.log("Uploading file", selectedFile);
    menu.uploadFile(selectedFile as unknown as File);
  };

  return (
    <div className="menu p-4 bg-base-100 border-t text-base-content">
      <div>
        <ul>
          {state.data.map((p, i) => (
            <li key={i} className="flex items-center">
              <DataEntry {...p} />
            </li>
          ))}
        </ul>
        <div className="divider"></div>
        <span>
          <input type="file" className="file-input w-full" accept="text/csv" onChange={fileHandler} />
          {selectedFile && (
            <button className="justify-center btn-xl" onClick={uploadFile}>
              <IoCloudUpload />
            </button>
          )}
        </span>
      </div>
    </div>
  );
};

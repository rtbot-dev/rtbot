import { ProgramEntry } from "./ProgramEntry";
import menu from "@/store/menu";
import { useLayoutEffect, useState } from "react";
import { IoAddCircle } from "react-icons/all";

export const Programs = () => {
  const [state, setState] = useState(menu.getState());
  useLayoutEffect(() => {
    menu.subscribe(setState);
  }, []);
  return (
    <div className="p-4 bg-base-100 text-base-content">
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
    </div>
  );
};

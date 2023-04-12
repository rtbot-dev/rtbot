import { ReactNode, useLayoutEffect, useState } from "react";
import windowsManager from "../store";
import { Window } from "./Window";
import { nanoid } from "nanoid";

export const WindowsManager = () => {
  const [state, setState] = useState(windowsManager.getState);
  useLayoutEffect(() => {
    windowsManager.subscribe(setState);
  });

  const addWindow = () => {
    const id = nanoid(3);
    console.log("Adding a new window", id);
    windowsManager.addWindow(
      {
        title: `Window ${id}`,
        zIndex: state.windows.length,
        initialStyle: {
          top: `${50 * state.windows.length}px`,
          left: `${100 * state.windows.length}px`,
          width: "300px",
          height: "400px",
        },
        children: <div>{`Hi from window ${id}, random number ${Math.random()}`}</div>,
      },
      id
    );
  };

  return (
    <div className="windows-manager" onDoubleClick={addWindow}>
      {state.windows.map((w) => {
        return (
          <Window {...w} key={`window-${w.id}`}>
            {w.children}
          </Window>
        );
      })}
    </div>
  );
};

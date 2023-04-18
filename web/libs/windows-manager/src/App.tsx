import React, { useLayoutEffect, useState } from "react";
import { windowsManager } from "./store";
import { WindowsManager } from "./components/WindowsManager";
import { nanoid } from "nanoid";
import { Window } from "./components/Window";

function App() {
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
    <div className="App" onDoubleClick={addWindow}>
      <WindowsManager>
        <Window
          title="this is the way"
          id="1"
          zIndex={0}
          initialStyle={{ width: "200px", height: "300px", top: "100px" }}
        >
          Hello, it's me
        </Window>
        <Window
          title="hello"
          id="2"
          zIndex={1}
          initialStyle={{ width: "200px", height: "300px", left: "100px" }}
          notClosable={true}
        >
          Hello, it's me
        </Window>
      </WindowsManager>
    </div>
  );
}

export default App;

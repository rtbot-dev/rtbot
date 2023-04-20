import { ReactNode, ReactPortal, useLayoutEffect, useState } from "react";
import { Window, WindowProps } from "./Window";
import { windowsManager } from "../store";

export type WindowsManagerProps = {
  children?: ReactNode;
};

export const WindowsManager = ({ children: windowsManagerChildren }: WindowsManagerProps) => {
  const [state, setState] = useState(windowsManager.getState);

  useLayoutEffect(() => {
    windowsManager.subscribe(setState);
    if (windowsManagerChildren) {
      const children: ReactNode[] = (windowsManagerChildren as any).length
        ? (windowsManagerChildren as ReactNode[])
        : [windowsManagerChildren];

      children.flat().forEach((r: any) => windowsManager.addWindow(r.props as WindowProps, r.props.id));
    }
  });

  return (
    <div className="windows-manager">
      {state.windows.map((w: WindowProps) => {
        return (
          <Window {...w} key={`window-${w.id}`}>
            {w.children}
          </Window>
        );
      })}
    </div>
  );
};

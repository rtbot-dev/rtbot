import React, { ReactNode, useLayoutEffect } from "react";
import Draggable from "react-draggable";
import { IoClose } from "react-icons/all";
import { windowsManager } from "../store";

export interface WindowProps {
  children: ReactNode;
  title: string;
  id: string;
  zIndex: number;
  minimized?: boolean;
  notClosable?: boolean;
  onClose?: (id: string) => void;
  initialStyle?: {
    width: string;
    height: string;
    top: string;
    left?: string;
    right?: string;
  };
}
export const Window = ({ title, children, initialStyle, id, zIndex, onClose, notClosable }: WindowProps) => {
  //useLayoutEffect()
  const initialWidth = initialStyle?.width ?? "200px";
  const initialHeight = initialStyle?.height ?? "400px";
  const initialTop = initialStyle?.top ?? "200px";
  let horizontal = {};
  if (initialStyle?.left) {
    horizontal = { left: initialStyle?.left };
  } else if (initialStyle?.right) {
    horizontal = { right: initialStyle?.right };
  }

  return (
    <Draggable handle=".window-title-bar">
      <div
        className="windows-body resize bg-slate-200"
        style={{
          position: "absolute",
          width: initialWidth,
          height: initialHeight,
          top: initialTop,
          zIndex,
          ...horizontal,
        }}
      >
        <div
          className="window-title-bar flex justify-center align-middle"
          onMouseDown={(event) => {
            windowsManager.bringWindowFront(id, zIndex);
          }}
        >
          <strong>{title}</strong>
          {!notClosable && (
            <button
              className="btn btn-ghost close-btn"
              onMouseDown={(event) => {
                event.stopPropagation();
                windowsManager.deleteWindow(id);
                if (onClose) onClose(id);
              }}
            >
              <IoClose></IoClose>
            </button>
          )}
        </div>

        <div
          className="bg-base-100"
          style={{ width: "100%", height: "100%" }}
          onMouseDown={(event) => {
            windowsManager.bringWindowFront(id, zIndex);
          }}
        >
          {children}
        </div>
      </div>
    </Draggable>
  );
};

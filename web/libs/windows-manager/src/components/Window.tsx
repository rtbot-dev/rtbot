import React, { ReactNode, useLayoutEffect } from "react";
import Draggable from "react-draggable";
import { IoClose } from "react-icons/all";
import windowsManager from "../store";

export interface WindowProps {
  children: ReactNode;
  title: string;
  id: string;
  zIndex: number;
  minimized?: boolean;
  initialStyle?: {
    width: string;
    height: string;
    top: string;
    left: string;
  };
}
export const Window = ({ title, children, initialStyle, id, zIndex }: WindowProps) => {
  //useLayoutEffect()
  const initialWidth = initialStyle?.width ?? "200px";
  const initialHeight = initialStyle?.height ?? "400px";
  const initialTop = initialStyle?.top ?? "200px";
  const initialLeft = initialStyle?.left ?? "200px";

  return (
    <Draggable handle=".window-title-bar">
      <div
        className="windows-body resize bg-base-300"
        style={{
          position: "absolute",
          width: initialWidth,
          height: initialHeight,
          top: initialTop,
          left: initialLeft,
          zIndex,
        }}
      >
        <div
          className="window-title-bar flex justify-center align-middle"
          onMouseDown={(event) => {
            windowsManager.bringWindowFront(id, zIndex);
          }}
        >
          <strong>{title}</strong>
          <button
            className="btn btn-ghost close-btn"
            onMouseDown={(event) => {
              event.stopPropagation();
              windowsManager.deleteWindow(id);
            }}
          >
            <IoClose></IoClose>
          </button>
        </div>

        <div
          className="bg-base-200"
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

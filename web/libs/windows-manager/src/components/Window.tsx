import React, { ReactNode } from "react";

export interface WindowProps {
  wrapped: ReactNode;
}
export const Window = ({ wrapped }: WindowProps) => {
  return (
    <div>
      this is a window!
      {wrapped}
    </div>
  );
};

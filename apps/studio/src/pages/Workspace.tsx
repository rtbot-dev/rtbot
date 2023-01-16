import { authenticated } from "./authenticated";

export const Workspace = authenticated(() => (
  <div className="flex items-center justify-center h-screen">Workspace</div>
));

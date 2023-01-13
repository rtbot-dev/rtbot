import "./index.css";
import React from "react";
import ReactDOM from "react-dom/client";
import { createBrowserRouter, RouterProvider } from "react-router-dom";
import { Error } from "@/pages/Error";
import { Profile } from "@/pages/Profile";
import { SignUp } from "@/pages/SignUp";
import { Workspace } from "@/pages/Workspace";

const router = createBrowserRouter([
  {
    path: "/",
    element: <SignUp />,
  },
  {
    path: "/error",
    element: <Error />,
  },
  {
    path: "/profile",
    element: <Profile />,
  },
  {
    path: "/workspace",
    element: <Workspace />,
  },
]);

ReactDOM.createRoot(document.getElementById("root") as HTMLElement).render(
  <React.StrictMode>
    <RouterProvider router={router} />
  </React.StrictMode>
);

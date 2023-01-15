import "./index.css";
import React from "react";
import ReactDOM from "react-dom/client";
import { createBrowserRouter, RouterProvider } from "react-router-dom";
import { Error } from "@/pages/Error";
import { Profile } from "@/pages/Profile";
import { SignUp } from "@/pages/SignUp";
import { Workspace } from "@/pages/Workspace";
import { SignIn } from "./pages/SignIn";

const router = createBrowserRouter([
  {
    path: "/",
    element: <SignIn />,
  },
  {
    path: "/signin",
    element: <SignIn />,
  },
  {
    path: "/signup",
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

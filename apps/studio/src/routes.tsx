import { createBrowserRouter } from "react-router-dom";
import { Error } from "@/pages/Error";
import { Profile } from "@/pages/Profile";
import { SignUp } from "@/pages/auth/SignUp";
import { Workspace } from "@/pages/workspace/Workspace";
import { SignIn } from "@/pages/auth/SignIn";

export const router = createBrowserRouter([
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

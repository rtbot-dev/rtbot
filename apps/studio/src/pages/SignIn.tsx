import { SignInForm } from "@/components/auth/SignInForm";
import { toPrivateLandingPage } from "./toPrivateLandingPage";

export const SignIn = toPrivateLandingPage(() => (
  <div className="flex items-center justify-center h-screen">
    <SignInForm />
  </div>
));

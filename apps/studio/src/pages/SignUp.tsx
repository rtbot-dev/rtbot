import { SignUpForm } from "@/components/auth/SignUpForm";
import { toPrivateLandingPage } from "./toPrivateLandingPage";

export const SignUp = toPrivateLandingPage(() => (
  <div className="flex items-center justify-center h-screen">
    <SignUpForm />
  </div>
));

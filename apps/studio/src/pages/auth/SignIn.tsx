import { SignInForm } from "@/pages/auth/components/SignInForm";
import { toPrivateLandingPage } from "@/pages/toPrivateLandingPage";

export const SignIn = toPrivateLandingPage(() => (
  <div className="flex items-center justify-center h-screen">
    <SignInForm />
  </div>
));

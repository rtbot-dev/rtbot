import { SignUpForm } from "@/pages/auth/components/SignUpForm";
import { toPrivateLandingPage } from "@/pages/toPrivateLandingPage";

export const SignUp = toPrivateLandingPage(() => (
  <div className="flex items-center justify-center h-screen">
    <SignUpForm />
  </div>
));

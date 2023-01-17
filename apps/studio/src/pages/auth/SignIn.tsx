import { SignInForm } from "@/pages/auth/components/SignInForm";
import { toPrivateLandingPage } from "@/pages/toPrivateLandingPage";
import { withErrorNotification } from "@/pages/withErrorNotification";

export const SignIn = withErrorNotification(
  toPrivateLandingPage(() => (
    <div className="flex items-center justify-center h-screen">
      <SignInForm />
    </div>
  ))
);

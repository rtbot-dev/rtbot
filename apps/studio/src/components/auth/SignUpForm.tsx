import React, { ReactElement, useEffect } from "react";
import { useState, useLayoutEffect } from "react";
import auth, { IAuthState, initialState } from "@/store/auth";
import { useNavigate } from "react-router-dom";
import { signUp } from "@/api/firebase";

const renderField = (child: ReactElement, state: IAuthState, key: keyof IAuthState) => {
  return (
    <React.Fragment>
      {child}
      {state[key].value && !state[key].valid && (
        <label className="block mb-2 text-sm font-light text-gray-900 dark:text-white">
          {state[key].invalidMessage}
        </label>
      )}
    </React.Fragment>
  );
};

export function SignUpForm() {
  const navigate = useNavigate();
  const [authState, setAuthState] = useState(initialState);

  useLayoutEffect(() => {
    auth.subscribe(setAuthState);
  }, []);

  const onFormSubmit = (e: React.FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    signUp();
  };

  useEffect(() => {
    if (authState.user) {
      // navigate to default page
      navigate("/workspace");
    }
  });

  const disabled = !authState.isSignUpFormValid;

  return (
    <section>
      <div></div>
      <div className="flex flex-col items-center justify-center px-6 py-8 mx-auto md:h-screen lg:py-0">
        <a href="#" className="flex items-center mb-6 text-2xl font-semibold text-gray-900 dark:text-white">
          <img className="w-16 h-16 mr-2" src="./rtbot-logo.svg" alt="logo" />
        </a>
        <div className="w-full bg-white rounded-lg shadow dark:border md:mt-0 sm:max-w-md xl:p-0 dark:bg-gray-800 dark:border-gray-700">
          <div className="p-6 space-y-4 md:space-y-6 sm:p-8">
            <h1 className="text-xl font-bold leading-tight tracking-tight text-gray-900 md:text-2xl dark:text-white">
              Create a new account
            </h1>
            <form onSubmit={onFormSubmit} className="space-y-4 md:space-y-6" action="#">
              <div>
                <label htmlFor="email" className="block mb-2 text-sm font-medium text-gray-900 dark:text-white">
                  Your email
                </label>
                {renderField(
                  <input
                    onChange={(event) => auth.setField("email", event.target.value)}
                    type="email"
                    name="email"
                    id="email"
                    className="bg-gray-50 border border-gray-300 text-gray-900 sm:text-sm rounded-lg focus:ring-primary-600 focus:border-primary-600 block w-full p-2.5 dark:bg-gray-700 dark:border-gray-600 dark:placeholder-gray-400 dark:text-white dark:focus:ring-blue-500 dark:focus:border-blue-500"
                    placeholder="name@company.com"
                  />,
                  authState,
                  "email"
                )}
              </div>
              <div>
                <label htmlFor="password" className="block mb-2 text-sm font-medium text-gray-900 dark:text-white">
                  Password
                </label>
                {renderField(
                  <input
                    onChange={(event) => auth.setField("password", event.target.value)}
                    type="password"
                    name="password"
                    id="password"
                    placeholder="••••••••"
                    className="bg-gray-50 border border-gray-300 text-gray-900 sm:text-sm rounded-lg focus:ring-primary-600 focus:border-primary-600 block w-full p-2.5 dark:bg-gray-700 dark:border-gray-600 dark:placeholder-gray-400 dark:text-white dark:focus:ring-blue-500 dark:focus:border-blue-500"
                  />,
                  authState,
                  "password"
                )}
              </div>
              <div>
                <label htmlFor="password" className="block mb-2 text-sm font-medium text-gray-900 dark:text-white">
                  Repeat password
                </label>
                {renderField(
                  <input
                    onChange={(event) => auth.setField("passwordRepeated", event.target.value)}
                    type="password"
                    name="passwordRepeated"
                    id="passwordRepeated"
                    placeholder="••••••••"
                    className="bg-gray-50 border border-gray-300 text-gray-900 sm:text-sm rounded-lg focus:ring-primary-600 focus:border-primary-600 block w-full p-2.5 dark:bg-gray-700 dark:border-gray-600 dark:placeholder-gray-400 dark:text-white dark:focus:ring-blue-500 dark:focus:border-blue-500"
                  />,
                  authState,
                  "passwordRepeated"
                )}
              </div>
              <div className="flex items-center justify-between">
                <a href="#" className="text-sm font-medium text-primary-600 hover:underline dark:text-primary-500">
                  Forgot password?
                </a>
              </div>
              <button
                disabled={disabled}
                type="submit"
                className="disabled:opacity-25 w-full text-white bg-primary-600 hover:bg-primary-700 focus:ring-4 focus:outline-none focus:ring-primary-300 font-medium rounded-lg text-sm px-5 py-2.5 text-center dark:bg-primary-600 dark:hover:bg-primary-700 dark:focus:ring-primary-800"
              >
                Sign up
              </button>
              <p className="text-sm font-light text-gray-500 dark:text-gray-400">
                Already have an account?
                <a
                  onClick={() => navigate("/signin")}
                  href="#"
                  className="font-medium text-primary-600 hover:underline dark:text-primary-500"
                >
                  Sign in
                </a>
              </p>
            </form>
          </div>
        </div>
      </div>
    </section>
  );
}

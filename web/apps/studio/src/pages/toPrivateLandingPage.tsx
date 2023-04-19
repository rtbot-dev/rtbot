import React, { useEffect, useLayoutEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import auth from "@/store/auth";

export const toPrivateLandingPage =
  (WrappedComponent: React.FC, props: any = {}) =>
    () => {
      const navigate = useNavigate();
      const [authState, setAuthState] = useState(auth.getState());

      useLayoutEffect(() => {
        auth.subscribe(setAuthState);
      }, []);

      useEffect(() => {
        if (authState.user) {
          // navigate to default page
          navigate("/workspace");
        }
      });
      return <WrappedComponent {...props} />;
    };

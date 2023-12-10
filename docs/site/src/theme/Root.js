import React from "react";
// Import the functions you need from the SDKs you need
import { initializeApp } from "firebase/app";
import { getAnalytics } from "firebase/analytics";
import ExecutionEnvironment from "@docusaurus/ExecutionEnvironment";
import useDocusaurusContext from "@docusaurus/useDocusaurusContext";

// Default implementation, that you can customize
export default function Root({ children }) {
  const {
    siteConfig: { customFields },
  } = useDocusaurusContext();
  if (ExecutionEnvironment.canUseDOM) {
    console.log("customFields", customFields);
    const firebaseConfig = {
      apiKey: customFields.firebaseApiKey,
      authDomain: "rtbot-6515d.firebaseapp.com",
      projectId: "rtbot-6515d",
      storageBucket: "rtbot-6515d.appspot.com",
      messagingSenderId: "454936526147",
      appId: "1:454936526147:web:875f1490bf6cfded5743f6",
      measurementId: "G-RZHQN042HN",
    };

    // Initialize Firebase
    const app = initializeApp(firebaseConfig);
    const analytics = getAnalytics(app);
  }

  return <>{children}</>;
}

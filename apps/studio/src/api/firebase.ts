import { initializeApp } from "firebase/app";
import {
  createUserWithEmailAndPassword,
  getAuth,
  GithubAuthProvider,
  signInWithEmailAndPassword,
  signInWithPopup,
} from "firebase/auth";
import authStore from "@/store/auth";
import errorStore from "@/store/error";

const firebaseConfig = {
  apiKey: "AIzaSyCqB0uKOBnMh85SgneXjLid9aOsCQmDnqU",
  authDomain: "rtbot-6515d.firebaseapp.com",
  projectId: "rtbot-6515d",
  storageBucket: "rtbot-6515d.appspot.com",
  messagingSenderId: "454936526147",
  appId: "1:454936526147:web:fda411d0b1ba72b35743f6",
};

// Initialize Firebase
export const app = initializeApp(firebaseConfig);

const auth = getAuth(app);
auth.onAuthStateChanged((user) => {
  console.log("auth state changed", user);
  authStore.setUser(user);
});

const githubProvider = new GithubAuthProvider();

export const signInWithGithub = () => {
  signInWithPopup(auth, githubProvider)
    .then((userCredential) => {
      // Signed in
      console.log("Signed in with github", userCredential);
    })
    .catch((error) => {
      errorStore.setError(error);
      console.error(`Unable to sign in user, error code: ${error.code}, ${error.message}`);
    });
};

export const signUp = () => {
  // remember to make sure that the form is valid before calling this function
  const authState = authStore.getState();
  const email = authState.email.value as string;
  const password = authState.password.value as string;

  createUserWithEmailAndPassword(auth, email, password)
    .then((userCredential) => {
      // Signed in
      console.log("Signed up", userCredential);
    })
    .catch((error) => {
      errorStore.setError(error);
      const errorCode = error.code;
      const errorMessage = error.message;
      console.error(`Unable to create user, error code: ${errorCode}, ${errorMessage}`);
    });
};

export const signIn = () => {
  // remember to make sure that the form is valid before calling this function
  const authState = authStore.getState();
  const email = authState.email.value as string;
  const password = authState.password.value as string;

  signInWithEmailAndPassword(auth, email, password)
    .then((userCredential) => {
      // Signed in
      console.log("Signed in", userCredential);
    })
    .catch((error) => {
      errorStore.setError(error);
      console.error(`Unable to sign in user, error code: ${error.code}, ${error.message}`);
    });
};

export const signOut = () => {
  auth
    .signOut()
    .then(() => {
      console.log("Signed out successfully");
    })
    .catch((error) => {
      errorStore.setError(error);
      console.error("Unable to sign out", error.code, error.message);
    });
};

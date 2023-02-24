import { Subject } from "rxjs";
import { FormField } from "./types";
import { z } from "zod";
import { User } from "firebase/auth";

const subject = new Subject<IAuthState>();

export interface IAuthState {
  email: FormField<string>;
  password: FormField<string>;
  passwordRepeated: FormField<string>;
  rememberMe: FormField<boolean>;
  isSignUpFormValid: boolean;
  isSignInFormValid: boolean;
  user: User | null;
}

export const initialState: IAuthState = {
  email: {},
  password: {},
  passwordRepeated: {},
  rememberMe: {},
  user: null,
  isSignUpFormValid: false,
  isSignInFormValid: false,
};

let state = initialState;

// zod schemas
const schema = z.object({
  email: z.string().email(),
  password: z.string().min(6).max(30),
  passwordRepeated: z.string().min(6).max(30),
  rememberMe: z.boolean(),
});

// store
export const store = {
  init: () => {
    console.log("Initializing auth state");
    state = { ...initialState };
    subject.next(state);
  },
  subscribe: (setState: (value: IAuthState) => void) => subject.subscribe(setState),
  setUser(user: User | null) {
    state.user = user;
    subject.next({ ...state });
  },
  setField: (field: keyof z.infer<typeof schema>, value: any) => {
    // check if valid
    const parsed = schema.pick({ [field]: true }).safeParse({ [field]: value });
    if (parsed.success) {
      console.log(`Field ${field} is valid: ${value}`);
      state[field] = {
        // @ts-ignore
        value: parsed.data[field],
        valid: true,
      };
    } else {
      state[field] = {
        value,
        valid: false,
        invalidMessage: parsed.error.issues[0].message,
      };
    }

    const formState = {
      email: state.email.value,
      password: state.password.value,
      passwordRepeated: state.passwordRepeated.value,
    };
    state.isSignUpFormValid = schema
      .pick({ email: true, password: true, passwordRepeated: true })
      .safeParse(formState).success;
    state.isSignInFormValid = schema.pick({ email: true, password: true }).safeParse(formState).success;
    if (field === "passwordRepeated") {
      // a valid value has to be equal to the password value
      state.passwordRepeated = {
        value,
        valid: state.password.value === value,
        invalidMessage: state.password.value === value ? "" : "Repeated password doesn't match",
      };
    }

    state.isSignUpFormValid = state.isSignUpFormValid && state.password.value === state.passwordRepeated.value;
    subject.next({ ...state });
  },
  getState: () => ({ ...state }),
};

store.init();

export default store;

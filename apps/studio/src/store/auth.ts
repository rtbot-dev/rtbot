import { Observer, Subject } from "rxjs";
import { FormField } from "./types";
import { z } from "zod";

const subject = new Subject<IAuthState>();

export interface IAuthState {
  email: FormField<string>;
  password: FormField<string>;
  passwordRepeated: FormField<string>;
  rememberMe: FormField<boolean>;
}

export const initialState: IAuthState = {
  email: {},
  password: {},
  passwordRepeated: {},
  rememberMe: {},
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
    console.log("initializing auth state");
    state = { ...initialState };
    subject.next(state);
  },
  subscribe: (setState: (value: IAuthState) => void) => subject.subscribe(setState),
  setField: (field: keyof z.infer<typeof schema>, value: any) => {
    // check if valid
    const parsed = schema.pick({ [field]: true }).safeParse({ [field]: value });
    console.log("parsed data", parsed);
    if (parsed.success) {
      console.log(`Field ${field} is valid: ${value}`);
      state[field] = {
        // @ts-ignore
        value: parsed.data[field],
        valid: true,
      };
    } else {
      console.log(`Field ${field} value ${value} is invalid ${parsed.error.issues[0].message}`);
      state[field] = {
        value,
        valid: false,
        invalidMessage: parsed.error.issues[0].message,
      };
    }

    if (field === "passwordRepeated") {
      // a valid value has to be equal to the password value
      state.passwordRepeated = {
        value,
        valid: state.password.value === value,
        invalidMessage: state.password.value === value ? "" : "Repeated password doesn't match",
      };
    }
    subject.next({ ...state });
  },
};

export default store;

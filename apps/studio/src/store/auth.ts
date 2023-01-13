import { Observer, Subject } from "rxjs";
import { FormField } from "./types";
import { z } from "zod";

const subject = new Subject<IAuthStore>();

export interface IAuthStore {
  email: FormField<string>;
  password: FormField<string>;
  rememberMe: FormField<boolean>;
}

export const initialState: IAuthStore = {
  email: {},
  password: {},
  rememberMe: {},
};

let state = initialState;

// zod schemas
const schema = {
  email: z.string().email(),
  password: z.string().min(6).max(30),
  rememberMe: z.boolean(),
};

// store
export const store = {
  init: () => {
    console.log("initializing auth state");
    state = { ...initialState };
    subject.next(state);
  },
  subscribe: (setState: (value: IAuthStore) => void) => subject.subscribe(setState),
  setField: (field: keyof typeof schema, value: any) => {
    // check if valid
    const parsed = schema[field].safeParse(value);
    if (parsed.success) {
      console.log(`Field ${field} is valid: ${value}`);
      state[field] = {
        value: parsed.data,
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
    subject.next({ ...state });
  },
};

export default store;

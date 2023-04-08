import { Subject } from "rxjs";

const subject = new Subject<IErrorState>();

export interface IErrorState {
  message?: string;
}

export const initialState: IErrorState = {};

let state = initialState;

// store
export const store = {
  init: () => {
    console.debug("Initializing error store");
    state = { ...initialState };
    subject.next(state);
  },
  subscribe: (setState: (value: IErrorState) => void) => subject.subscribe(setState),
  setError(error: IErrorState = {}) {
    subject.next({ message: error.message });
    // remove automatically after a while
    setTimeout(() => {
      subject.next({});
    }, 5000);
  },
  getState: () => ({ ...state }),
};

store.init();

export default store;

import { Subject } from "rxjs";
import { Program } from "@/store/editor/schemas";
import { programApi } from "@/api/program";
import auth from "./auth";

const subject = new Subject<IMenuState>();

export interface IMenuState {
  sideMenuOpen: boolean;
  message?: string;
  programs: Program[];
}

export const initialState: IMenuState = {
  sideMenuOpen: false,
  programs: [],
};

let state = initialState;

const refreshProgramList = () => {
  programApi.list().then((programs) => {
    console.log("list of programs", programs);
    state.programs = programs;
    subject.next({ ...state });
  });
};

auth.subscribe(({ user }) => {
  console.log("Refreshing program list");
  if (user) refreshProgramList();
});
// store
export const store = {
  init: () => {
    console.debug("Initializing menu store");
    state = { ...initialState };
    subject.next(state);
  },
  subscribe: (setState: (value: IMenuState) => void) => subject.subscribe(setState),
  hide() {
    state.sideMenuOpen = false;
    subject.next({ ...state });
  },
  toggle() {
    state.sideMenuOpen = !state.sideMenuOpen;
    subject.next({ ...state });
    refreshProgramList();
  },
  createProgram(title: string = "New program") {
    programApi
      .create({
        metadata: {
          title,
        },
        connections: [],
        operators: [],
      })
      .then(refreshProgramList);
  },
  deleteProgram(programId: string) {
    programApi.delete(programId).then(refreshProgramList);
  },
  getState: () => ({ ...state }),
};

store.init();

export default store;

import { Subject } from "rxjs";
import { Program } from "@/store/editor/schemas";
import { programApi } from "@/api/program";

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
// store
export const store = {
  init: () => {
    console.debug("Initializing menu store");
    state = { ...initialState };
    subject.next(state);
  },
  subscribe: (setState: (value: IMenuState) => void) => subject.subscribe(setState),
  toggleSideMenu() {
    state.sideMenuOpen = !state.sideMenuOpen;
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

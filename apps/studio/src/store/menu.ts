import { Subject } from "rxjs";
import { Program } from "@/store/editor/schemas";
import { programApi } from "@/api/program";
import auth from "./auth";
import { Data, dataApi } from "@/api/data";

const subject = new Subject<IMenuState>();

export interface IMenuState {
  sideMenuOpen: boolean;
  message?: string;
  programs: Program[];
  data: Data[];
  editingProgramList: boolean;
  editingDataList: boolean;
  uploadingFile: boolean;
  uploadProgress?: number;
}

export const initialState: IMenuState = {
  sideMenuOpen: false,
  editingProgramList: false,
  editingDataList: false,
  uploadingFile: false,
  programs: [],
  data: [],
};

let state = initialState;

const refreshProgramList = () => {
  programApi.list().then((programs) => {
    console.log("list of programs", programs);
    state.programs = programs;
    subject.next({ ...state });
  });
};

const refreshDataList = () => {
  dataApi.list().then((data) => {
    console.log("list of data", data);
    state.data = data;
    subject.next({ ...state });
  });
};
auth.subscribe(({ user }) => {
  console.log("Refreshing program list");
  if (user) {
    refreshProgramList();
    refreshDataList();
  }
});
// store
export const store = {
  init: () => {
    console.debug("Initializing menu store");
    state = { ...initialState };
    subject.next(state);
  },
  subscribe: (setState: (value: IMenuState) => void) => subject.subscribe(setState),
  setUploadProgress(progress: number) {
    state.uploadProgress = progress;
    subject.next({ ...state });
  },
  hide() {
    state.sideMenuOpen = false;
    subject.next({ ...state });
  },
  toggle() {
    state.sideMenuOpen = !state.sideMenuOpen;
    subject.next({ ...state });
    refreshProgramList();
    refreshDataList();
  },
  createProgram(title: string = "New program") {
    state = { ...state, editingProgramList: true };
    subject.next(state);
    programApi
      .create({
        metadata: {
          title,
        },
        connections: [],
        operators: [],
      })
      .then(() => {
        state = { ...state, editingProgramList: false };
        subject.next(state);
        refreshProgramList();
      });
  },
  deleteProgram(programId: string) {
    state = { ...state, editingProgramList: true };
    subject.next(state);
    programApi.delete(programId).then(() => {
      state = { ...state, editingProgramList: false };
      subject.next(state);
      refreshProgramList();
    });
  },
  uploadFile(file: File) {
    state = { ...state, uploadingFile: true, uploadProgress: 0 };
    subject.next({ ...state });
    dataApi.uploadFile(file).then(() => {
      state = { ...state, uploadingFile: false, uploadProgress: 0 };
      subject.next({ ...state });
      refreshDataList();
    });
  },
  deleteData(dataId: string) {
    state = { ...state, editingDataList: true };
    subject.next(state);
    dataApi.delete(dataId).then(() => {
      state = { ...state, editingDataList: false };
      subject.next(state);
      refreshDataList();
    });
  },
  getState: () => ({ ...state }),
};

store.init();

export default store;

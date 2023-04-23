import { Subject } from "rxjs";
import { Program } from "@/store/editor/schemas";
import { programApi } from "@/api/program";
import auth from "./auth";
import { Data, dataApi } from "@/api/data";
import { Simulate } from "react-dom/test-utils";
import progress = Simulate.progress;

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
auth.subscribe(({ user }) => {
  console.log("Refreshing program list");
  if (user) {
    store.refreshProgramList();
    store.refreshDataList();
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

  refreshProgramList() {
    programApi.list().then((programs) => {
      console.log("list of programs", programs);
      state.programs = programs;
      subject.next({ ...state });
    });
  },
  refreshDataList() {
    dataApi.list().then((data) => {
      console.log("list of data", data);
      state.data = data;
      subject.next({ ...state });
    });
  },
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
    this.refreshProgramList();
    this.refreshDataList();
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
        this.refreshProgramList();
      });
  },
  deleteProgram(programId: string) {
    state = { ...state, editingProgramList: true };
    subject.next(state);
    programApi.delete(programId).then(() => {
      state = { ...state, editingProgramList: false };
      subject.next(state);
      this.refreshProgramList();
    });
  },
  uploadFile(file: File) {
    state = { ...state, uploadingFile: true, uploadProgress: 0 };
    subject.next({ ...state });
    dataApi
      .uploadFile(file, (progress) => {
        state = { ...state, uploadProgress: progress };
        subject.next({ ...state });
      })
      .then(() => {
        state = { ...state, uploadingFile: false, uploadProgress: 0 };
        subject.next({ ...state });
        this.refreshDataList();
      });
  },
  updateDataTitle(dataId: string, title: string) {
    state = { ...state, editingDataList: true };
    subject.next(state);
    dataApi
      .update(dataId, {
        title,
      })
      .then(() => {
        state = { ...state, editingDataList: false };
        subject.next({ ...state });
        this.refreshDataList();
      });
  },
  deleteData(dataId: string) {
    state = { ...state, editingDataList: true };
    subject.next(state);
    dataApi.delete(dataId).then(() => {
      state = { ...state, editingDataList: false };
      subject.next(state);
      this.refreshDataList();
    });
  },
  loadData(dataId: string) {
    dataApi.load(dataId).then(() => {
      console.log("TODO: data loaded, update state");
    });
  },
  getState: () => ({ ...state }),
};

store.init();

export default store;

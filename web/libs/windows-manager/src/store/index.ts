import { Subject } from "rxjs";
import { WindowProps } from "../components/Window";
import { nanoid } from "nanoid";

const subject = new Subject<IWindowsManagerState>();

export interface IWindowsManagerState {
  windows: WindowProps[];
}

export const initialState: IWindowsManagerState = {
  windows: [],
};

let state: IWindowsManagerState = initialState;

// store
export const store = {
  init: () => {
    console.debug("Initializing windowsManager store");
    state = { ...initialState };
    subject.next(state);
  },
  addWindow(windowProps: Omit<WindowProps, "id">, id?: string) {
    state.windows.push({ ...windowProps, id: id ?? nanoid(3) });
    subject.next({ ...state });
  },
  updateWindow(windowProps: Omit<WindowProps, "id">, id: string) {
    state.windows = state.windows.reduce(
      (acc: WindowProps[], w) => [...acc, id === w.id ? { ...w, ...windowProps } : w],
      []
    );
    subject.next({ ...state });
  },
  bringWindowFront(id: string, zIndex: number) {
    state.windows = state.windows.reduce(
      (acc: WindowProps[], w) => [
        ...acc,
        { ...w, zIndex: w.id === id ? state.windows.length - 1 : w.zIndex > zIndex ? w.zIndex - 1 : w.zIndex },
      ],
      []
    );
    subject.next({ ...state });
  },
  deleteWindow(id: string) {
    console.log("Deleting window", id);
    // recall that we need to update the zIndex as well when deleting a window
    const { zIndex } = state.windows.find((w) => w.id === id)!;
    state.windows = state.windows
      .reduce(
        (acc: WindowProps[], w) => [
          ...acc,
          { ...w, zIndex: w.id === id ? state.windows.length - 1 : w.zIndex > zIndex ? w.zIndex - 1 : w.zIndex },
        ],
        []
      )
      .filter((w) => w.id !== id);
    subject.next({ ...state });
  },
  subscribe: (setState: (value: IWindowsManagerState) => void) => subject.subscribe(setState),
  getState: () => ({ ...state }),
};

store.init();

export default store;

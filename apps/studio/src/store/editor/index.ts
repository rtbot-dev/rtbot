import { Subject } from "rxjs";
import { Program, programSchema } from "./schemas";
import zodToJsonSchema from "zod-to-json-schema";

const subject = new Subject<IEditorState>();

export interface IEditorState {
  sourceCode: string;
  program?: Program;
}

export const initialState: IEditorState = {
  sourceCode: "",
};

let state = initialState;

// store
export const store = {
  init: () => {
    console.debug("Initializing editor store");
    state = { ...initialState };
    subject.next(state);
  },
  subscribe: (setState: (value: IEditorState) => void) => subject.subscribe(setState),
  setEditor(editor: IEditorState = { sourceCode: "" }) {
    subject.next({ ...editor });
  },
  setSourceCode(sourceCode: string) {
    subject.next({ ...state, sourceCode });
  },
  addOperator(operatorDef: any) {},
  getState: () => ({ ...state }),
};

store.init();

export default store;

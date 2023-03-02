import { Subject } from "rxjs";
import { Program, programSchema } from "./schemas";
import zodToJsonSchema from "zod-to-json-schema";
import { BaseOperator, Metadata } from "./operator.schemas";

const subject = new Subject<IEditorState>();

export interface IEditorState {
  sourceCode: string;
  program?: Program;
}

export const initialState: IEditorState = {
  sourceCode: "",
  program: {
    title: "new program",
    operators: [],
    connections: [],
  },
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
  setEditor(editor: IEditorState = initialState) {
    subject.next({ ...editor });
  },
  setSourceCode(sourceCode: string) {
    state = { ...state, sourceCode };
    subject.next({ ...state });
  },
  editOperator(id: string, value: boolean) {
    if (state.program) {
      state = {
        ...state,
        program: {
          ...state.program,
          operators: state.program.operators.reduce(
            (acc, op) => [...acc, op.id === id ? { ...op, metadata: { ...op.metadata, editing: value } } : { ...op }],
            []
          ),
        },
      };
      subject.next({ ...state });
    }
  },
  addOperator(operator: Partial<BaseOperator>) {
    if (state.program) {
      state = {
        ...state,
        program: {
          ...state.program,
          operators: [...state.program.operators, { ...operator }],
        },
      };
      subject.next({ ...state });
    }
  },
  updateOperator(operator: BaseOperator) {
    if (state.program) {
      state = {
        ...state,
        program: {
          ...state.program,
          operators: state.program.operators.reduce(
            (acc, op) => [...acc, op.id === operator.id ? { ...op, ...operator } : op],
            []
          ),
        },
      };
      subject.next({ ...state });
    }
  },
  incrementOperatorPosition(operatorId: string, dx: number, dy: number) {
    if (state.program) {
      state = {
        ...state,
        program: {
          ...state.program,
          operators: state.program.operators.reduce(
            (acc, op) => [
              ...acc,
              op.id === operatorId
                ? {
                    ...op,
                    metadata: {
                      ...op.metadata,
                      position: {
                        x: op.metadata.position.x + dx,
                        y: op.metadata.position.y + dy,
                      },
                    },
                  }
                : op,
            ],
            []
          ),
        },
      };
      subject.next({ ...state });
    }
  },
  deleteOperator(operator: BaseOperator) {
    if (state.program) {
      state = {
        ...state,
        program: {
          ...state.program,
          operators: state.program.operators.filter((op) => op.id !== operator.id),
          connections: state.program.connections.filter((con) => con.from !== operator.id && con.to !== operator.id),
        },
      };
      subject.next({ ...state });
    }
  },
  deleteConnection(from: string, to: string) {
    if (state.program) {
      state = {
        ...state,
        program: {
          ...state.program,
          connections: state.program.connections.filter((con) => con.from !== from || con.to !== to),
        },
      };
      subject.next({ ...state });
    }
  },
  addConnection(from: string, to: string) {
    if (state.program) {
      state = {
        ...state,
        program: {
          ...state.program,
          connections: [...state.program.connections, { from, to }],
        },
      };
      subject.next({ ...state });
    }
  },
  getState: () => ({ ...state }),
};

store.init();

export default store;

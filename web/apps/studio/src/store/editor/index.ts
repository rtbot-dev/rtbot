import { Subject } from "rxjs";
import { Program } from "./schemas";
import { BaseOperator } from "./operator.schemas";
import { programApi } from "../../api/program";

const subject = new Subject<IEditorState>();

export interface IEditorState {
  sourceCode: string;
  program: Program | null;
}

export const initialState: IEditorState = {
  sourceCode: "",
  program: null,
};

let state = initialState;

const save = async () => {
  if (state.program && state.program.metadata && state.program.metadata.id) {
    return programApi.update(state.program.metadata.id, {
      connections: state.program.connections,
      operators: state.program.operators,
    });
  }
};

// @see https://stackoverflow.com/questions/27936772/how-to-deep-merge-instead-of-shallow-merge
export const merge = (objFrom: any, objTo: any) =>
  Object.keys(objFrom).reduce(
    (merged, key) => {
      merged[key] =
        objFrom[key] instanceof Object && !Array.isArray(objFrom[key])
          ? merge(objFrom[key], merged[key] ?? {})
          : objFrom[key];
      return merged;
    },
    { ...objTo }
  );
let debounce: NodeJS.Timeout | null = null;
// store
export const store = {
  init: () => {
    console.debug("Initializing editor store");
    state = { ...initialState };
    subject.next(state);
  },
  subscribe: (setState: (value: IEditorState) => void) => subject.subscribe(setState),
  setProgram(program: Program) {
    state.program = program;
    subject.next({ ...state });
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
      // persist the change
      save().then(() => console.log("Program saved"));
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
  updateOperator(operator: Partial<BaseOperator>) {
    if (state.program) {
      state = {
        ...state,
        program: {
          ...state.program,
          operators: state.program.operators.reduce(
            (acc: BaseOperator[], op: BaseOperator) => [
              ...acc,
              op.id === operator.id
                ? { ...op, ...operator, metadata: merge(operator.metadata ?? {}, op.metadata ?? {}) }
                : op,
            ],
            []
          ),
        },
      };
      // persist the change
      save().then(() => console.log("Program saved"));
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
            (acc: BaseOperator[], op: BaseOperator) => [
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
      // as this method will be called many times per second, we will debounce it and
      // make the save call after 1 sec of the last change
      if (debounce) clearTimeout(debounce);

      debounce = setTimeout(() => {
        // persist the change
        console.log("Saving");
        save().then(() => console.log("Program saved"));
        debounce = null;
      }, 1000);
      subject.next({ ...state });
    }
  },
  deleteOperator(operator: { id: string }) {
    if (state.program) {
      state = {
        ...state,
        program: {
          ...state.program,
          operators: state.program.operators.filter((op) => op.id !== operator.id),
          connections: state.program.connections.filter((con) => con.from !== operator.id && con.to !== operator.id),
        },
      };
      // persist the change
      save().then(() => console.log("Program saved"));
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
      // persist the change
      save().then(() => console.log("Program saved"));
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
      // persist the change
      save().then(() => console.log("Program saved"));
      subject.next({ ...state });
    }
  },
  getState: () => ({ ...state }),
};

store.init();

export default store;

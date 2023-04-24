import { Subject } from "rxjs";
import { Program } from "./schemas";
import { BaseOperator } from "./operator.schemas";
import { programApi } from "../../api/program";
import { rtbotApi } from "@/api/rtbot/rtbot.api";
import plot from "../plot";

const subject = new Subject<IEditorState>();

export interface IEditorState {
  // Notice that this is a list of *opened* programs, not the list of
  // all user programs, hence this is a subset of the last
  programs: Program[];
}

export const initialState: IEditorState = {
  programs: [],
};

let state = initialState;

const save = async (programId: string) => {
  const program = state.programs.find((p) => p.metadata?.id === programId);
  if (program) {
    return programApi.update(programId, {
      connections: program.connections,
      operators: program.operators,
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

export enum OperatorForm {
  DEF = "def",
  PLOT = "plot",
}
// store
export const store = {
  init: () => {
    console.debug("Initializing editor store");
    state = { ...initialState };
    subject.next(state);
  },
  subscribe: (setState: (value: IEditorState) => void) => subject.subscribe(setState),
  openProgram(program: Program) {
    state.programs = [...state.programs, program];
    subject.next({ ...state });
  },
  closeProgram(programId: string) {
    state.programs = state.programs.filter((p) => p.metadata?.id !== programId);
    subject.next({ ...state });
  },
  editOperator(programId: string, operatorId: string, form?: OperatorForm) {
    console.log("Editing operator ", programId, operatorId, form);
    const program = state.programs.find((p) => p.metadata?.id === programId);
    console.log("associated program", program);
    if (program) {
      const programs: Program[] = state.programs.reduce(
        (programs: Program[], p: Program) => [
          ...programs,
          p.metadata?.id === programId
            ? {
                ...program,
                operators: program.operators.reduce(
                  (ops: BaseOperator[], op: BaseOperator) => [
                    ...ops,
                    op.id === operatorId ? { ...op, metadata: { ...op.metadata, editing: form ?? false } } : { ...op },
                  ],
                  []
                ),
              }
            : p,
        ],
        []
      );
      state = {
        ...state,
        programs,
      };
      // persist the change
      save(programId).then(() => console.log("Program saved"));
      subject.next({ ...state });
    }
  },
  addOperator(programId: string, operator: BaseOperator) {
    const program = state.programs.find((p) => p.metadata?.id === programId);
    if (program) {
      const programs: Program[] = state.programs.reduce(
        (programs: Program[], p: Program) => [
          ...programs,
          p.metadata?.id === programId
            ? {
                ...p,
                operators: [...p.operators, { ...operator }],
              }
            : p,
        ],
        []
      );
      state = {
        ...state,
        programs,
      };
      // persist the change
      save(programId).then(() => console.log("Program saved"));
      subject.next({ ...state });
    }
  },
  updateOperator(programId: string, operator: Partial<BaseOperator>) {
    const program = state.programs.find((p) => p.metadata?.id === programId);
    if (program) {
      const programs: Program[] = state.programs.reduce(
        (programs: Program[], p: Program) => [
          ...programs,
          p.metadata?.id === programId
            ? {
                ...program,
                operators: program.operators.reduce(
                  (ops: BaseOperator[], op: BaseOperator) => [
                    ...ops,
                    op.id === operator.id
                      ? { ...op, ...operator, metadata: merge(operator.metadata ?? {}, op.metadata ?? {}) }
                      : op,
                  ],
                  []
                ),
              }
            : p,
        ],
        []
      );
      state = {
        ...state,
        programs,
      };
      // persist the change
      save(programId).then(() => console.log("Program saved"));
      subject.next({ ...state });
    }
  },
  incrementOperatorPosition(programId: string, operatorId: string, dx: number, dy: number) {
    const program = state.programs.find((p) => p.metadata?.id === programId);
    if (program) {
      const programs: Program[] = state.programs.reduce(
        (programs: Program[], p: Program) => [
          ...programs,
          p.metadata?.id === programId
            ? {
                ...program,
                operators: program.operators.reduce(
                  (ops: BaseOperator[], op: BaseOperator) => [
                    ...ops,
                    op.id === operatorId
                      ? {
                          ...op,
                          metadata: {
                            ...op.metadata,
                            position: {
                              x: (op.metadata?.position?.x ?? 0) + dx,
                              y: (op.metadata?.position?.y ?? 0) + dy,
                            },
                          },
                        }
                      : op,
                  ],
                  []
                ),
              }
            : p,
        ],
        []
      );
      state = {
        ...state,
        programs,
      };
      // as this method will be called many times per second, we will debounce it and
      // make the save call after 1 sec of the last change
      if (debounce) clearTimeout(debounce);

      debounce = setTimeout(() => {
        // persist the change
        console.log("Saving");
        save(programId).then(() => console.log("Program saved"));
        debounce = null;
      }, 1000);
      subject.next({ ...state });
    }
  },
  deleteOperator(programId: string, operator: { id: string }) {
    const program = state.programs.find((p) => p.metadata?.id === programId);
    if (program) {
      const programs: Program[] = state.programs.reduce(
        (programs: Program[], p: Program) => [
          ...programs,
          p.metadata?.id === programId
            ? {
                ...p,
                operators: p.operators.filter((op) => op.id != operator.id),
              }
            : p,
        ],
        []
      );
      state = {
        ...state,
        programs,
      };
      // persist the change
      save(programId).then(() => console.log("Program saved"));
      subject.next({ ...state });
    }
  },
  deleteConnection(programId: string, from: string, to: string) {
    const program = state.programs.find((p) => p.metadata?.id === programId);
    if (program) {
      const programs: Program[] = state.programs.reduce(
        (programs: Program[], p: Program) => [
          ...programs,
          p.metadata?.id === programId
            ? {
                ...p,
                connections: p.connections.filter((con) => con.from !== from || con.to !== to),
              }
            : p,
        ],
        []
      );
      state = {
        ...state,
        programs,
      };
      // persist the change
      save(programId).then(() => console.log("Program saved"));
      subject.next({ ...state });
    }
  },
  // TODO: generalize this to consider explicit connections between operator ports
  addConnection(programId: string, from: string, to: string, sourceHandle?: string, targetHandle?: string) {
    const program = state.programs.find((p) => p.metadata?.id === programId);
    if (program) {
      const toPort = targetHandle ? { toPort: parseInt(targetHandle) } : {};
      const programs: Program[] = state.programs.reduce(
        (programs: Program[], p: Program) => [
          ...programs,
          p.metadata?.id === programId
            ? {
                ...p,
                connections: [
                  ...p.connections
                    // remove dangling connections, if any
                    .filter(
                      (c) =>
                        p.operators.map((o) => o.id).indexOf(c.from) > -1 &&
                        p.operators.map((o) => o.id).indexOf(c.to) > -1
                    )
                    // remove previous connection, if exist
                    .filter((c) => c.from !== from || c.to !== to),
                  { from, to, ...toPort },
                ],
              }
            : p,
        ],
        []
      );
      state = {
        ...state,
        programs,
      };
      // persist the change
      save(programId).then(() => console.log("Program saved"));
      subject.next({ ...state });
    }
  },
  setProgramComputing(programId: string, computing: boolean) {
    const programs: Program[] = state.programs.reduce(
      (programs: Program[], p: Program) => [
        ...programs,
        p.metadata?.id === programId ? { ...p, metadata: { ...p.metadata, computing } } : p,
      ],
      []
    );
    state = { ...state, programs };
    subject.next({ ...state });
  },
  run(programId: string, dataId: string) {
    console.log("Running program", programId);
    // notice that we use the same program id as the plot id
    // this may change in the future but I think is ok for now
    // notice also that if the plot doesn't exist at the time
    // this won't cause any issue
    plot.setPlotComputing(programId, true);
    const program = state.programs.find((p) => p.metadata?.id === programId);
    if (program) {
      this.setProgramComputing(programId, true);
      rtbotApi
        .run(program, dataId)
        .then((outputs) => {
          console.log("outputs", outputs);
          plot.upsertPlot(programId, outputs, program.metadata?.title ?? "Output", program.operators);
          plot.setPlotComputing(programId, false);
          this.setProgramComputing(programId, false);
        })
        .catch((e) => console.error("Something went wrong while running program", program, "with data", dataId, e));
    }
  },
  getState: () => ({ ...state }),
};

store.init();

export default store;

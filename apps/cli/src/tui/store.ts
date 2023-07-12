import { createStore } from "zustand/vanilla";
import { produce } from "immer";
import { Program, ExtendedFormat } from "@rtbot/api";

export type DebuggerStateActions = {
  forward: () => void;
  backward: () => void;
  init: (program: Program, result: ExtendedFormat) => void;
  updateProgram: (program: Program) => void;
  updateResult: (result: ExtendedFormat) => void;
  updateIteration: (iteration: number) => void;
  selectOperator: (id: string, selected: boolean) => void;
  updateOperator: (id: string, selected: boolean) => void;
  computeColumns: () => void;
  computeOperators: () => void;
};
export type DebuggerOperatorDef = { id: string; opType: string; selected: boolean };

export type DebuggerState = {
  iteration: number;
  timeCursor: number;
  maxIteration: number;
  program?: Program;
  result?: ExtendedFormat;
  // model of left select operator form
  operators: DebuggerOperatorDef[];
  // model of output table
  columns: string[][];
};

export const debuggerStore = createStore<DebuggerState & DebuggerStateActions>((set, get) => ({
  timeCursor: -1,
  iteration: 0,
  maxIteration: 0,
  operators: [],
  columns: [],
  init: (program: Program, result: ExtendedFormat) => {
    // recall that the order here matters
    get().updateProgram(program);
    get().updateResult(result);
    get().computeOperators();
    get().computeColumns();
  },
  updateResult: (result: ExtendedFormat) => set((_) => ({ result: [...result], maxIteration: result.length })),
  updateProgram: (program: Program) => set((_) => ({ program })),
  computeOperators: () =>
    set((state) => ({
      operators: state.program!.operators.map((op: any, i: number) => ({
        id: op.id,
        opType: op.opType,
        selected: true,
      })),
    })),
  updateOperator: (id: string, selected: boolean) =>
    set((state) => ({
      operators: produce(state.operators, (draft) => {
        const index = draft.findIndex((op) => op.id === id);
        if (index !== -1) draft[index]!.selected = selected;
      }),
    })),
  computeColumns: () =>
    set((state) => {
      const operators = state.operators.filter((op) => op.selected).map((op) => op.id);
      const rows = [];
      const { iteration, maxIteration } = state;

      const headers = ["iter", ...operators];

      if (iteration === 0)
        return {
          columns: [headers],
        };

      for (let i = iteration - 1; i < Math.min(iteration, maxIteration) + 10; i++) {
        const r = state.result![i]!;
        const row = operators.map((h) => {
          if (r.out[h]) {
            return r.out[h]!.map(({ time, value }: any) => `(${time}, ${Math.round(value * 100) / 100})`).join(", ");
          }
          return "-";
        });
        rows.push([`${i}`, ...row]);
      }
      return {
        columns: [headers, ...rows],
      };
    }),
  selectOperator: (id: string, selected: boolean) => {
    get().updateOperator(id, selected);
    get().computeColumns();
  },
  updateIteration: (iteration: number) => set((_) => ({ iteration })),
  forward: () => {
    const { iteration, maxIteration } = get();
    get().updateIteration(Math.min(iteration + 1, maxIteration));
    get().computeColumns();
  },
  backward: () => {
    const { iteration } = get();
    get().updateIteration(Math.max(iteration - 1, 0));
    get().computeColumns();
  },
}));

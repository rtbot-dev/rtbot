import { Program, ExtendedFormat } from "@rtbot/api";
import { outputTable } from "./output-table";
import { selectOperatorsForm } from "./select-operators-form";
import { DebuggerOperatorDef, debuggerStore } from "./store";
const blessed = require("blessed");

export const tui = (program: Program, result: ExtendedFormat) => {
  const screen = blessed.screen({ smartCSR: true, log: "output.log", debug: true });

  screen.log("initializing");

  // Create a box perfectly centered horizontally and vertically.
  const selectOperatorsFormBox = selectOperatorsForm();
  const outputTableBox = outputTable();
  screen.append(selectOperatorsFormBox);
  screen.append(outputTableBox);

  debuggerStore.getState().init(program, result);

  let playTimer: NodeJS.Timeout | undefined;

  // move in time
  screen.key(["right"], function (_ch: string, _key: string) {
    debuggerStore.getState().forward();
  });
  screen.key(["left"], function (_ch: string, _key: string) {
    debuggerStore.getState().backward();
  });
  screen.key(["p"], function (_ch: string, _key: string) {
    if (playTimer) clearInterval(playTimer);
    playTimer = setInterval(() => {
      debuggerStore.getState().forward();
    }, 200);
  });
  screen.key(["b"], function (_ch: string, _key: string) {
    if (playTimer) clearInterval(playTimer);
    playTimer = setInterval(() => {
      debuggerStore.getState().backward();
    }, 200);
  });
  screen.key(["s"], function (_ch: string, _key: string) {
    clearInterval(playTimer);
  });

  // Quit on Escape, q, or Control-C.
  screen.key(["escape", "q", "C-c"], function (_ch: string, _key: string) {
    return process.exit(0);
  });

  // we need this otherwise rendering is not consistent with state changes
  // we need a debounce here so this don't get called in excess
  // while we long press the right arrow button
  const render = debounce(() => screen.render(), 100);
  debuggerStore.subscribe((_) => {
    render();
  });
  screen.render();
};

function debounce(func: any, timeout = 300) {
  let timer: any;
  return (...args: any[]) => {
    clearTimeout(timer);
    timer = setTimeout(() => {
      // @ts-ignore
      func.apply(this, args);
    }, timeout);
  };
}

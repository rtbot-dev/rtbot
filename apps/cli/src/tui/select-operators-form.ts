const blessed = require("blessed");
import { debuggerStore } from "./store";

export const selectOperatorsForm = () => {
  const box = blessed.box({
    top: "0",
    left: "0",
    width: 30,
    label: " Select operators ",
    height: "100%",
    tags: true,
    border: {
      type: "line",
    },
  });
  const selectOperatorsForm = blessed.form({
    parent: box,
  });

  debuggerStore.subscribe((state) => {
    state.operators.forEach((opDef, i: number) => {
      const checkbox = blessed.checkbox({
        parent: selectOperatorsForm,
        text: `${opDef.id} [${opDef.opType}]`,
        top: i,
        mouse: true,
        keys: true,
        shrink: true,
        checked: opDef.selected,
        padding: {
          left: 1,
          right: 1,
        },
      });
      checkbox.on("check", function () {
        debuggerStore.getState().selectOperator(opDef.id, true);
      });
      checkbox.on("uncheck", function () {
        debuggerStore.getState().selectOperator(opDef.id, false);
      });
    });
  });

  return box;
};

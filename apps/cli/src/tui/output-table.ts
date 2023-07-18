const blessed = require("blessed");
import { debuggerStore } from "./store";

export const outputTable = () => {
  const box = blessed.box({
    top: "0",
    right: "0",
    label: " Outputs ",
    width: "100%-30",
    height: "100%",
    tags: true,
    border: {
      type: "line",
    },
  });
  const table = blessed.listtable({
    parent: box,
    style: {
      header: {
        bold: true,
      },
    },
  });

  debuggerStore.subscribe((state) => {
    if (state.columns.length > 0) {
      // console.log("state columns", state.columns);
      table.setData(state.columns);
    }
  });

  return box;
};

import * as Comlink from "comlink";
import { Program } from "@/store/editor/schemas";
import { dataApi } from "@/api/data";

export const rtbotApi = {
  async run(program: Program, dataId: string) {
    const data = await dataApi.load(dataId);

    const RtBotRun = Comlink.wrap(new Worker(new URL("./rtbot.worker.ts", import.meta.url), { type: "module" }));
    const rtbotRun = await new RtBotRun(program, data);
    await rtbotRun.run();
    const outputs = await rtbotRun.getOutputs();
    console.log("outputs from comlink wrapped class", outputs);
    return outputs;
  },
};

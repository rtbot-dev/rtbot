// mock implementation of the library
import { Program } from "@/store/editor/schemas";
import { dataApi } from "@/api/data";
import { RtBotRun } from "./rtbot-run";

export const rtbotApi = {
  async run(program: Program, dataId: string) {
    const data = await dataApi.load(dataId);
    // TODO: run this using web workers
    const rtbotRun = new RtBotRun(program, data);
    await rtbotRun.run();
    const outputs = rtbotRun.getOutputs();
    console.log("rtbot api outputs", outputs);
    return outputs;
  },
};

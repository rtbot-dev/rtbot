import bindings, { RtBotEmbindModule } from "@rtbot/wasm";

export interface RtBotIterationOutput {
  [operatorId: string]: RtBotMessage[];
}

export interface RtBotMessage {
  time: number;
  value: number;
}

export class RtBot {
  private rtbot: Promise<RtBotEmbindModule>;

  constructor() {
    this.rtbot = bindings();
  }

  async createPipeline(pipelineId: string, programStr: string): Promise<string> {
    return (await this.rtbot).createPipeline(pipelineId, programStr);
  }

  async deletePipeline(pipelineId: string): Promise<string> {
    return (await this.rtbot).deletePipeline(pipelineId);
  }

  async sendMessage(pipelineId: string, port: string, time: number, value: number): Promise<RtBotIterationOutput> {
    return JSON.parse((await this.rtbot).receiveMessageInPipelineDebug(pipelineId, time, value));
  }
}

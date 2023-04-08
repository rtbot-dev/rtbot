import * as Comlink from "comlink";
import { Program } from "@/store/editor/schemas";
import { nanoid } from "nanoid";
import bindings from "@rtbot/core";

interface RtBotMessage {
  time: number;
  value: number[];
}

interface RtBot {
  createPipeline(pipelineId: string, programStr: string): string;
  deletePipeline(pipelineId: string): string;
  receiveMessageInPipelineDebug(pipelineId: string, time: number, value: number[]): string;
}

export interface RtBotIterationOutput {
  [operatorId: string]: RtBotMessage;
}

export class RtBotRun {
  // this variable will hold the outputs from different
  // operators in columnar format: the first element will be
  // a list with all the timestamp produced by the operator output
  // the second element will be a list with all the first values
  // of the operator output, and so on.
  private readonly outputs: { [operatorId: string]: number[][] };
  constructor(private readonly program: Program, private readonly data: number[][]) {
    this.outputs = {};
  }

  private getRtBotProgram() {
    const operators = this.program.operators.map((op) => ({ ...op, ...op.parameters, type: op.opType }));
    const connections = this.program.connections.filter((c) => c.from !== null && c.to !== null);
    return JSON.stringify({ ...this.program, operators, connections });
  }

  async run() {
    const rtbot: RtBot = await bindings();
    const pipelineId = nanoid(10);
    const rtbotProgram = this.getRtBotProgram();
    console.log("Creating pipeline", pipelineId, "program\n", rtbotProgram);
    rtbot.createPipeline(pipelineId, rtbotProgram);
    console.log("Pipeline", pipelineId, "created, sending data to it");

    // iterate over the data passed and send it to the rtbot program
    this.data.forEach(([time, ...value]) => {
      const iterationOutput = rtbot.receiveMessageInPipelineDebug(pipelineId, time, value);
      //console.log("iteration output", iterationOutput);
      // record the outputs
      Object.entries(JSON.parse(iterationOutput) as RtBotIterationOutput).forEach(([k, { time, value }]) => {
        if (!this.outputs[k]) this.outputs[k] = [[], ...value.map((_) => [])];

        // add the time to the first list in the output
        this.outputs[k][0].push(time);
        // add the values to the correspondent lists in the output
        value.forEach((v, i) => this.outputs[k][i + 1].push(v));
      });
    });
    console.log("Deleting pipeline");
    rtbot.deletePipeline(pipelineId);
    console.log("Done!", this.outputs);
  }

  getOutputs() {
    return this.outputs;
  }
}

Comlink.expose(RtBotRun);
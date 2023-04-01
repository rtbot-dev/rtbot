import { Program } from "@/store/editor/schemas";

interface RtBotMessage {
  time: number;
  value: number[];
}
// This class should be replaced with the actual implementation
// coming from the rtbot core c++ code and its bindings
class RtBot {
  private inputId: string = "";
  createPipeline(program: Program): string {
    // find the first input
    const inputOp = program.operators.find((o) => o.opType === "INPUT");

    if (!inputOp) {
      console.log("Please pass at least one input operator");
      return "";
    }
    this.inputId = inputOp.id;
    return "pipelineId";
  }
  receiveMessageInPipeline(pipelineId: string, message: RtBotMessage): RtBotIterationOutput {
    // for every message passed return it as if were passed through the input operator
    return { [this.inputId]: message };
  }
}

export interface RtBotIterationOutput {
  [operatorId: string]: RtBotMessage;
}

export class RtBotRun {
  private readonly pipelineId: string;
  private readonly rtbot: RtBot;
  // this variable will hold the outputs from different
  // operators in columnar format: the first element will be
  // a list with all the timestamp produced by the operator output
  // the second element will be a list with all the first values
  // of the operator output, and so on.
  private readonly outputs: { [operatorId: string]: number[][] };
  constructor(private readonly program: Program, private readonly data: number[][]) {
    this.rtbot = new RtBot();
    this.pipelineId = this.rtbot.createPipeline(program);
    this.outputs = {};
  }

  run() {
    // iterate over the data passed and send it to the rtbot program
    this.data.forEach(([time, ...value]) => {
      const iterationOutput = this.rtbot.receiveMessageInPipeline(this.pipelineId, { time, value });
      // record the outputs
      Object.entries(iterationOutput).forEach(([k, { time, value }]) => {
        if (!this.outputs[k]) this.outputs[k] = [[], ...value.map((_) => [])];

        // add the time to the first list in the output
        this.outputs[k][0].push(time);
        // add the values to the correspondent lists in the output
        value.forEach((v, i) => this.outputs[k][i + 1].push(v));
      });
    });
  }

  getOutputs() {
    return this.outputs;
  }
}

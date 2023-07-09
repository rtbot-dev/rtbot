import bindings, { RtBotEmbindModule } from "@rtbot/wasm";
import { Operator, Program } from "./program";

export interface RtBotIterationOutput {
  [operatorId: string]: RtBotMessage[];
}

export interface RtBotMessage {
  time: number;
  value: number;
}

export class RtBot {
  private static instance?: RtBot;

  static getInstance() {
    if (!this.instance) {
      this.instance = new RtBot();
    }
    return this.instance;
  }

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

  async sendMessage(pipelineId: string, time: number, value: number): Promise<RtBotIterationOutput> {
    return JSON.parse((await this.rtbot).receiveMessageInPipelineDebug(pipelineId, time, value));
  }
}

export enum RtBotRunOutputFormat {
  COLLAPSED,
  EXTENDED,
}

type CollapsedFormat = { [operatorId: string]: number[][] };
type ExtendedFormat = { in: RtBotMessage; out: { [operatorId: string]: RtBotMessage[] } }[];

export class RtBotRun {
  // this variable will hold the outputs from different
  // operators in columnar format: the first element will be
  // a list with all the timestamp produced by the operator output
  // the second element will be a list with all the first values
  // of the operator output, and so on.
  private readonly outputs: CollapsedFormat | ExtendedFormat;

  constructor(
    private readonly program: Program,
    private readonly data: number[][],
    private readonly format: RtBotRunOutputFormat = RtBotRunOutputFormat.COLLAPSED,
    private readonly verbose: boolean = false
  ) {
    if (format === RtBotRunOutputFormat.COLLAPSED) this.outputs = {};
    else this.outputs = [];
  }

  async run() {
    this.program.validate();
    const program = this.program.toPlain();
    // wasm code expects "type" instead of "opType" field used here
    program.operators = (program.operators as Operator[]).map((op) =>
      Object.keys(op as any).reduce(
        // @ts-ignore
        (acc, k) => (k === "opType" ? { ...acc, type: op.opType } : { ...acc, [`${k}`]: op[k] }),
        {}
      )
    );
    const programStr = JSON.stringify(program, null, 2);
    if (this.verbose) console.log("Sending", programStr);
    await RtBot.getInstance().createPipeline(this.program.programId, programStr);

    if (this.verbose) console.log("Sending data...");
    // iterate over the data passed and send it to the rtbot program
    await Promise.all(
      this.data.map(async ([time, ...value]) => {
        // TODO generalize to the case where we have several inputs
        const iterationOutput = await RtBot.getInstance().sendMessage(this.program.programId, time, value[0]);
        if (this.verbose) console.log("iteration ", time, value, "=>", iterationOutput);
        // record the outputs
        if (this.format === RtBotRunOutputFormat.COLLAPSED) {
          Object.entries(iterationOutput as RtBotIterationOutput).forEach(([k, msgs]) => {
            msgs.forEach(({ time, value }) => {
              if (!(this.outputs as CollapsedFormat)[k]) (this.outputs as CollapsedFormat)[k] = [[], []];

              // add the time to the first list in the output
              (this.outputs as CollapsedFormat)[k][0].push(time);
              // add the values to the correspondent lists in the output
              (this.outputs as CollapsedFormat)[k][1].push(value);
            });
          });
        } else {
          (this.outputs as ExtendedFormat).push({ in: { time, value: value[0] }, out: iterationOutput });
        }
      })
    );

    await RtBot.getInstance().deletePipeline(this.program.programId);
    if (this.verbose) console.log("Done!", this.outputs);
  }

  getOutputs() {
    return this.outputs;
  }
}

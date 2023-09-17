import bindings, { RtBotEmbindModule } from "@rtbot-dev/wasm";
import { Operator, Program } from "./program";

export interface RtBotIterationOutput {
  [operatorId: string]: { [port: string]: RtBotMessage[] };
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

  async createProgram(programId: string, programStr: string): Promise<string> {
    return (await this.rtbot).createProgram(programId, programStr);
  }

  async deleteProgram(programId: string): Promise<string> {
    return (await this.rtbot).deleteProgram(programId);
  }

  async processDebug(
    programId: string,
    messages: { [portId: string]: { time: number; value: number }[] }
  ): Promise<RtBotIterationOutput> {
    this.prepareInternalMessageBuffer(programId, messages, await this.rtbot);
    return JSON.parse((await this.rtbot).processMessageBufferDebug(programId));
  }

  async process(
    programId: string,
    messages: { [portId: string]: { time: number; value: number }[] }
  ): Promise<RtBotIterationOutput> {
    this.prepareInternalMessageBuffer(programId, messages, await this.rtbot);
    return JSON.parse((await this.rtbot).processMessageBuffer(programId));
  }

  prepareInternalMessageBuffer(
    programId: string,
    messages: { [portId: string]: { time: number; value: number }[] },
    rtbot: RtBotEmbindModule
  ) {
    Object.entries(messages).map(([port, msgs]) =>
      msgs.map(({ time, value }) => rtbot.addToMessageBuffer(programId, port, time, value))
    );
  }

  async getProgramEntryPorts(programId: string) {
    return (await this.rtbot).getProgramEntryPorts(programId);
  }
}

export enum RtBotRunOutputFormat {
  COLLAPSED,
  EXTENDED,
}

export type CollapsedFormat = { [operatorId: string]: number[][] };
export type ExtendedFormat = { in: { [portId: string]: RtBotMessage[] }; out: RtBotIterationOutput }[];

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
    const programStr = JSON.stringify(program, null, 2);
    if (this.verbose) console.log("Sending", programStr);
    const createProgramResponseStr = await RtBot.getInstance().createProgram(this.program.programId, programStr);

    if (createProgramResponseStr) {
      const createProgramResponse = JSON.parse(createProgramResponseStr);
      // if program fails validation, throw an error
      if (createProgramResponse.error) throw new Error(createProgramResponse.error);
    }

    if (this.verbose) console.log("Sending data...");
    const entryPorts: string[] = JSON.parse(await RtBot.getInstance().getProgramEntryPorts(this.program.programId));
    // iterate over the data passed and send it to the rtbot program
    await Promise.all(
      this.data.map(async ([time, ...values]) => {
        const msgs = Object.fromEntries(entryPorts.map((p, i) => [p, [{ time, value: values[i] }]]));
        const iterationOutput = await RtBot.getInstance().processDebug(this.program.programId, msgs);
        if (this.verbose) console.log("iteration ", time, values, "=>", iterationOutput);
        // record the outputs
        if (this.format === RtBotRunOutputFormat.COLLAPSED) {
          Object.entries(iterationOutput as RtBotIterationOutput).forEach(([opId, opOut]) => {
            Object.entries(opOut).forEach(([port, msgs]) => {
              const k = `${opId}:${port}`;
              msgs.forEach(({ time, value }) => {
                if (!(this.outputs as CollapsedFormat)[k]) (this.outputs as CollapsedFormat)[k] = [[], []];

                // add the time to the first list in the output
                (this.outputs as CollapsedFormat)[k][0].push(time);
                // add the values to the correspondent lists in the output
                (this.outputs as CollapsedFormat)[k][1].push(value);
              });
            });
          });
        } else {
          (this.outputs as ExtendedFormat).push({ in: msgs, out: iterationOutput });
        }
      })
    );

    await RtBot.getInstance().deleteProgram(this.program.programId);
    if (this.verbose) console.log("Done!", this.outputs);
  }

  getOutputs() {
    return this.outputs;
  }
}

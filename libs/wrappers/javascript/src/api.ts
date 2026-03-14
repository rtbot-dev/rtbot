import bindings, { RtBotEmbindModule } from "@rtbot-dev/wasm";
import { Program } from "./program";

export interface RtBotIterationOutput {
  [operatorId: string]: { [port: string]: RtBotMessage[] };
}

export interface RtBotMessage {
  time: number;
  value: number | number[] | boolean | boolean[];
}

type RtBotInputMessage = {
  time: number;
  value: number | number[];
};

type ProgramOperator = {
  id?: string;
  type?: string;
  portTypes?: string[];
  [key: string]: unknown;
};

type ProgramPlain = {
  entryOperator?: string;
  operators?: ProgramOperator[];
  [key: string]: unknown;
};

function isVectorPortType(portType: string | undefined): boolean {
  return portType === "vector_number" || portType === "vector_boolean";
}

function inferEntryPortTypes(program: ProgramPlain, entryPorts: string[]): string[] {
  if (!Array.isArray(program.operators) || entryPorts.length === 0) {
    return entryPorts.map(() => "number");
  }

  const entryOperatorId =
    typeof program.entryOperator === "string" && program.entryOperator.length > 0
      ? program.entryOperator
      : undefined;

  if (!entryOperatorId) {
    return entryPorts.map(() => "number");
  }

  const entryOperator = program.operators.find((operator) => operator.id === entryOperatorId);
  const portTypes = Array.isArray(entryOperator?.portTypes) ? entryOperator.portTypes : [];
  return entryPorts.map((_, index) => {
    const portType = portTypes[index];
    return typeof portType === "string" ? portType : "number";
  });
}

function buildIterationMessages(
  entryPorts: string[],
  entryPortTypes: string[],
  time: number,
  values: number[]
): { [portId: string]: RtBotInputMessage[] } {
  const vectorPortIndexes = entryPortTypes
    .map((portType, index) => (isVectorPortType(portType) ? index : -1))
    .filter((index) => index >= 0);

  if (vectorPortIndexes.length > 0) {
    if (!(entryPorts.length === 1 && vectorPortIndexes.length === 1 && vectorPortIndexes[0] === 0)) {
      throw new Error("Vector entry ports currently require a single entry input port.");
    }

    return {
      [entryPorts[0]]: [{ time, value: values }],
    };
  }

  const messages: { [portId: string]: RtBotInputMessage[] } = {};
  entryPorts.forEach((port, index) => {
    const value = values[index];
    if (!Number.isFinite(value)) {
      throw new Error(`Missing or invalid scalar value for port ${port} at index ${index}`);
    }
    messages[port] = [{ time, value }];
  });

  return messages;
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
    messages: { [portId: string]: RtBotInputMessage[] }
  ): Promise<RtBotIterationOutput> {
    this.prepareInternalMessageBuffer(programId, messages, await this.rtbot);
    return JSON.parse((await this.rtbot).processMessageBufferDebug(programId));
  }

  async process(
    programId: string,
    messages: { [portId: string]: RtBotInputMessage[] }
  ): Promise<RtBotIterationOutput> {
    this.prepareInternalMessageBuffer(programId, messages, await this.rtbot);
    return JSON.parse((await this.rtbot).processMessageBuffer(programId));
  }

  prepareInternalMessageBuffer(
    programId: string,
    messages: { [portId: string]: RtBotInputMessage[] },
    rtbot: RtBotEmbindModule
  ) {
    Object.entries(messages).forEach(([port, msgs]) => {
      msgs.forEach(({ time, value }) => {
        if (Array.isArray(value)) {
          const started = rtbot.beginVectorMessage(programId, port, time);
          if (started !== "1") {
            throw new Error(`Failed to start vector message for ${port}`);
          }

          let completed = false;
          try {
            value.forEach((vectorValue) => {
              if (!Number.isFinite(vectorValue)) {
                throw new Error(`Vector message for ${port} contains non-finite value`);
              }
              const pushed = rtbot.pushVectorMessageValue(programId, port, vectorValue);
              if (pushed !== "1") {
                throw new Error(`Failed to append vector value for ${port}`);
              }
            });

            const ended = rtbot.endVectorMessage(programId, port);
            if (ended !== "1") {
              throw new Error(`Failed to finalize vector message for ${port}`);
            }
            completed = true;
          } finally {
            if (!completed) {
              rtbot.abortVectorMessage(programId, port);
            }
          }
        } else {
          const added = rtbot.addToMessageBuffer(programId, port, time, value);
          if (added !== "1") {
            throw new Error(`Failed to queue scalar message for ${port}`);
          }
        }
      });
    });
  }

  async getProgramEntryPorts(programId: string) {
    return (await this.rtbot).getProgramEntryPorts(programId);
  }
}

export enum RtBotRunOutputFormat {
  COLLAPSED,
  EXTENDED,
}

export type CollapsedFormat = { [operatorId: string]: unknown[][] };
export type ExtendedFormat = { in: { [portId: string]: RtBotInputMessage[] }; out: RtBotIterationOutput }[];

export class RtBotRun {
  // this variable will hold the outputs from different
  // operators in columnar format: the first element will be
  // a list with all the timestamp produced by the operator output
  // the second element will be a list with all the first values
  // of the operator output, and so on.
  private readonly outputs: CollapsedFormat | ExtendedFormat;
  private readonly program: Program;

  constructor(
    program: Program | string,
    private readonly data: number[][],
    private readonly format: RtBotRunOutputFormat,
    private readonly progressCb: ((progress: number) => void) | undefined = undefined,
    private readonly verbose: boolean = false
  ) {
    if (typeof program === "string") this.program = Program.toInstance(JSON.parse(program));
    else this.program = program;
    if (format === RtBotRunOutputFormat.COLLAPSED) this.outputs = {};
    else this.outputs = [];
  }

  async run() {
    // const { success } = this.program.safeValidate();
    // if (!success) throw new Error(`Program is invalid`);
    const plainProgram = this.program.toPlain() as ProgramPlain;
    const programStr = JSON.stringify(plainProgram, null, 2);
    try {
      if (this.verbose) console.log("Sending", programStr);
      const createProgramResponseStr = await RtBot.getInstance().createProgram(this.program.programId, programStr);
      if (this.verbose) console.log("createProgram response", createProgramResponseStr);

      if (createProgramResponseStr) {
        try {
          const createProgramResponse = JSON.parse(createProgramResponseStr);
          if (createProgramResponse.error) {
            throw new Error(createProgramResponse.error);
          }
        } catch {
          throw new Error(createProgramResponseStr);
        }
      }
    } catch (e) {
      console.error("Error creating program", e);
      throw e;
    }

    if (this.verbose) console.log("Sending data...");
    const entryPorts: string[] = JSON.parse(await RtBot.getInstance().getProgramEntryPorts(this.program.programId));
    const entryPortTypes = inferEntryPortTypes(plainProgram, entryPorts);

    // iterate over the data passed and send it to the rtbot program
    const dataSize = this.data.length;
    const progressStepSize = Math.max(1, Math.floor(dataSize / 20));
    let progressStepCounter = 0;
    for (let i = 0; i < dataSize; i++) {
      const [time, ...values] = this.data[i];
      const msgs = buildIterationMessages(entryPorts, entryPortTypes, time, values);
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
      if (this.progressCb) {
        progressStepCounter++;
        if (progressStepCounter === progressStepSize) {
          progressStepCounter = 0;
          this.progressCb(i / dataSize);
        }
      }
    }

    await RtBot.getInstance().deleteProgram(this.program.programId);
    if (this.verbose) console.log("Done!", this.outputs);
  }

  getOutputs() {
    return this.outputs;
  }
}

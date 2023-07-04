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

const ppgProgram = `
{
    "title": "Peak detector",
    "description": "This is a program to detect peaks in PPG...",
    "date": "now",
    "apiVersion": "v1",
    "author": "Someone <someone@gmail.com>",
    "license": "MIT",
    "operators": [
        {
            "id": "in1",
            "type": "Input"
        },
        {
            "id": "ma1",
            "type": "MovingAverage",
            "n": 6
        },
        {
            "id": "ma2",
            "type": "MovingAverage",
            "n": 250
        },
        {
            "id": "minus",
            "type": "Minus"
        },
        {
            "id": "peak",
            "type": "PeakDetector",
            "n": 13
        },
        {
            "id": "join",
            "type": "Join",
            "numPorts": 2
        },
        {
            "id": "out1",
            "type": "Output"
        }
    ],
    "connections": [
        {
            "from": "in1",
            "to": "ma1",
            "fromPort": "o1",
            "toPort": "i1"
        },
        {
            "from": "ma1",
            "to": "minus",
            "fromPort": "o1",
            "toPort": "i1"
        },
        {
            "from": "minus",
            "to": "peak",
            "fromPort": "o1",
            "toPort": "i1"
        },
        {
            "from": "peak",
            "to": "join",
            "fromPort": "o1",
            "toPort": "i1"
        },
        {
            "from": "join",
            "to": "out1",
            "fromPort": "o2",
            "toPort": "i1"
        },
        {
            "from": "in1",
            "to": "ma2",
            "fromPort": "o1",
            "toPort": "i1"
        },
        {
            "from": "ma2",
            "to": "minus",
            "fromPort": "o1",
            "toPort": "i2"
        },
        {
            "from": "in1",
            "to": "join",
            "fromPort": "o1",
            "toPort": "i2"
        }
    ]
}
`;
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
    console.log("Sending", programStr);
    await RtBot.getInstance().createPipeline(this.program.programId, programStr);

    console.log("Sending data...");
    // iterate over the data passed and send it to the rtbot program
    await Promise.all(
      this.data.map(async ([time, ...value]) => {
        // TODO generalize to the case where we have several inputs
        const iterationOutput = await RtBot.getInstance().sendMessage(this.program.programId, time, value[0]);
        console.log("iteration ", time, value, "=>", iterationOutput);
        // record the outputs
        Object.entries(iterationOutput as RtBotIterationOutput).forEach(([k, msgs]) => {
          msgs.forEach(({ time, value }) => {
            if (!this.outputs[k]) this.outputs[k] = [[], []];

            // add the time to the first list in the output
            this.outputs[k][0].push(time);
            // add the values to the correspondent lists in the output
            this.outputs[k][1].push(value);
          });
        });
      })
    );

    await RtBot.getInstance().deletePipeline(this.program.programId);
    console.log("Done!", this.outputs);
  }

  getOutputs() {
    return this.outputs;
  }
}

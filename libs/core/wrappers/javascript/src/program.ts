import { Exclude, instanceToPlain, plainToInstance } from "class-transformer";
import { programSchema } from "./generated";
import { nanoid } from "nanoid";
import { RtBot } from "./api";

export type OperatorId = string;
export type PortId = string;

export class Program {
  operators: Operator[] = [];
  connections: Connection[] = [];
  programId: string;
  defaultPort?: string;

  constructor(
    public entryOperator?: string,
    readonly title?: string,
    readonly description?: string,
    readonly date?: string,
    readonly author?: string,
    readonly license?: string,
    readonly apiVersion: string = "v1"
  ) {
    this.programId = nanoid(10);
  }

  static checkValid(programJson: string) {
    const plain: Record<keyof any, unknown> = JSON.parse(programJson);
    const program: Program = plainToInstance(Program, plain);
    program.validate();
  }

  static toInstance(obj: Record<keyof any, unknown>): Program {
    return plainToInstance(Program, obj);
  }

  toPlain(): Record<keyof any, unknown> {
    return instanceToPlain(this);
  }

  validate() {
    programSchema.parse(JSON.parse(JSON.stringify(this)));
  }

  safeValidate() {
    return programSchema.safeParse(JSON.parse(JSON.stringify(this)));
  }

  addOperator(op: Operator) {
    op.setProgram(this);
    this.operators.push(op);
    if (op.type === "Input") {
      this.entryOperator = op.id;
    }
  }

  deleteOperator(opId: OperatorId) {
    this.operators = this.operators.filter((op) => op.id !== opId);
    this.connections = this.connections.filter(({ from, to }) => from !== opId && to !== opId);
  }

  deleteConnection(opFrom: OperatorId, opTo: OperatorId, opFromPort: PortId = "o1", opToPort: PortId = "i1") {
    this.connections = this.connections.filter(
      ({ from, to, fromPort, toPort }) =>
        !(from !== opFrom && to !== opTo && fromPort !== opFromPort && toPort !== opToPort)
    );
  }

  addConnection(opFrom: Operator, opTo: Operator, opFromPort: PortId = "o1", opToPort: PortId = "i1") {
    if (!this.operators.find((op) => op.id === opFrom.id))
      throw new Error(`From operator ${opFrom.id} hasn't been added to the program`);
    if (!this.operators.find((op) => op.id === opTo.id))
      throw new Error(`To operator ${opTo.id} hasn't been added to the program`);

    if (
      this.connections.find(
        ({ from, to, fromPort, toPort }) =>
          from === opFrom.id && to === opTo.id && fromPort === opFromPort && toPort === opToPort
      )
    )
      throw new Error(`There is already a connection from ${opFrom.id}:${opFromPort} to ${opTo.id}:${opToPort}`);
    this.connections.push(new Connection(opFrom.id, opTo.id, opFromPort, opToPort));
  }

  // operational methods, these are supposed to be use in production
  // for development purposes the `RtBotRun` class provides a better api
  async start() {
    this.validate();
    const plain = this.toPlain();
    const programStr = JSON.stringify(plain, null, 2);
    const createProgramResponseStr = await RtBot.getInstance().createProgram(this.programId, programStr);

    if (createProgramResponseStr) {
      const createProgramResponse = JSON.parse(createProgramResponseStr);
      // if program fails validation, throw an error
      if (createProgramResponse.error) throw new Error(createProgramResponse.error);
    }
    // set the default port
    this.defaultPort = JSON.parse(await RtBot.getInstance().getProgramEntryPorts(this.programId))[0];
  }

  async processMessageDebug(time: number, value: number, port?: string) {
    port = port ?? this.defaultPort;
    if (port) return await RtBot.getInstance().processDebug(this.programId, { [`${port}`]: [{ time, value }] });
    else throw new Error("Please specify an entry port to send the message");
  }

  async stop() {
    await RtBot.getInstance().deleteProgram(this.programId);
  }
}

export class Connection {
  constructor(readonly from: OperatorId, readonly to: OperatorId, readonly fromPort: PortId, readonly toPort: PortId) {}
}

export abstract class Operator {
  abstract type: string;
  @Exclude()
  private program?: Program;

  setProgram(program: Program) {
    this.program = program;
  }

  constructor(readonly id: string) {}

  toJSON() {
    return instanceToPlain(this);
  }
}

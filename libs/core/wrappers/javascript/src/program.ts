import { Exclude, instanceToPlain, plainToInstance } from "class-transformer";
import { programSchema } from "./generated";
import { nanoid } from "nanoid";

export type OperatorId = string;
export type PortId = string;

export class Program {
  operators: Operator[] = [];
  connections: Connection[] = [];
  programId: string;

  constructor(
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
    // recall that internally we use `opType` instead of `type`
    obj.operators = (obj.operators as any[]).reduce((acc, op) => {
      op.opType = op.type;
      delete op.type;
      return [...acc, op];
    }, []);
    return plainToInstance(Program, obj);
  }

  toPlain(): Record<keyof any, unknown> {
    return instanceToPlain(this);
  }

  validate() {
    programSchema.parse(JSON.parse(JSON.stringify(this)));
  }

  addOperator(op: Operator) {
    op.setProgram(this);
    this.operators.push(op);
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
}

export class Connection {
  constructor(readonly from: OperatorId, readonly to: OperatorId, readonly fromPort: PortId, readonly toPort: PortId) {}
}

export abstract class Operator {
  abstract opType: string;
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

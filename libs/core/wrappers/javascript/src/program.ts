import { Exclude, instanceToPlain, plainToInstance } from "class-transformer";
import { ZodSchema } from "zod";
import { programSchema } from "./generated"

export type OperatorId = string;
export type PortId = string;

export class Program {
  operators: Operator[] = [];
  connections: Connection[] = [];
  entryNode?: string;

  constructor(
    readonly title?: string,
    readonly description?: string,
    readonly date?: string,
    readonly author?: string,
    readonly license?: string,
    readonly apiVersion: string = "v1",
  ) {}

  static checkValid(programJson: string) {
    const plain: Record<keyof any, unknown> = JSON.parse(programJson);
    const program: Program = plainToInstance(Program, plain)
    program.validate()
  }

  validate() {
    programSchema.parse(JSON.parse(JSON.stringify(this)))
  }

  setEntryNode(entryNode: string) {
    this.entryNode = entryNode;
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
    if (
      !this.connections.find(
        ({ from, to, fromPort, toPort }) =>
          from === opFrom.id && to === opTo.id && fromPort === opFromPort && toPort === opToPort
      )
    )
      this.connections.push(new Connection(opFrom.id, opTo.id, opFromPort, opToPort));
    else
      console.warn(
        "There is already a connection from",
        opFrom,
        "to",
        opTo,
        " from port",
        opFromPort,
        "to port",
        opToPort
      );
  }
}

export class Connection {
  constructor(readonly from: OperatorId, readonly to: OperatorId, readonly fromPort: PortId, readonly toPort: PortId) {}
}

export abstract class Operator {
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

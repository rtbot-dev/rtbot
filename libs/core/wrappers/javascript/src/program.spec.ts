import { Program, MovingAverage, Input, Output, RtBotRun } from "@rtbot/api";

const data = [
  [1, 10.0],
  [2, 20.0],
  [3, 30.0],
  [4, 25.0],
  [5, 20.0],
  [6, 15.0],
];

describe("Program", () => {
  let program: Program;
  const title = "my program title";
  const description = "description";

  beforeEach(() => {
    program = new Program(title, description);
    const input = new Input("input1");
    const op1 = new MovingAverage("ma1", 2);
    const output = new Output("out1");
    program.addOperator(input);
    program.addOperator(op1);
    program.addOperator(output);
    program.addConnection(input, op1);
    program.addConnection(op1, output);
  });

  it("can create a new instance", () => {
    expect(program.title).toBe(title);
  });

  it("can be serialized", () => {
    const json = JSON.stringify(program);
    const parsedProgram = JSON.parse(json);
    expect(parsedProgram.operators.find((op: any) => op.id === "ma1")).toMatchObject({ id: "ma1", n: 2 });
  });

  it("validates operator input", () => {
    const wrap = () => {
      new MovingAverage("ma", 0.5);
    };
    expect(wrap).toThrow(/(.*)invalid_type(.*)/);
    expect(wrap).toThrow(/(.*)Expected integer, received float(.*)/);
  });

  it("validates the entire program", () => {
    const wrap = () => {
      program.validate();
    };
    expect(wrap).not.toThrow();
  });

  it("can process data", async () => {
    const rtbotRun = new RtBotRun(program, data);
    const result = await rtbotRun.run();
    console.log("result", result);
  });

  it("there has to be at least one Input operator in a program to be valid", () => {
    expect(1).toBe(2);
  });

  it("you cannot add a connection between unexisting operators", () => {
    expect(1).toBe(2);
  });
});

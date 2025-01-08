import { Program, MovingAverage, Input, Output, RtBotRun, RtBotRunOutputFormat } from "@rtbot-dev/rtbot";

const data = [
  [1, 10.0],
  [2, 20.0],
  [3, 30.0],
  [4, 25.0],
  [5, 20.0],
  [6, 15.0],
];

const programJson = `{
  "title": "Peak detector",
  "description": "This is a program to detect peaks in PPG...",
  "date": "now",
  "apiVersion": "v1",
  "author": "Someone <someone@gmail.com>",
  "license": "MIT",
  "operators": [
    {
      "id": "in1",
      "type": "Input",
      "portTypes": ["number"]
    },
    {
      "id": "ma1",
      "type": "MovingAverage",
      "window_size": 6
    },
    {
      "id": "ma2",
      "type": "MovingAverage",
      "window_size": 250
    },
    {
      "id": "minus",
      "type": "Subtraction"
    },
    {
      "id": "peak",
      "type": "PeakDetector",
      "window_size": 13
    },
    {
      "id": "join",
      "type": "Join",
      "portTypes": ["number", "number"]
    },
    {
      "id": "out1",
      "type": "Output",
      "portTypes": ["number"]
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
  ],
  "output": {
    "out1": ["o1"]
  }
}
`;

describe("Program", () => {
  let program: Program;
  const title = "my program title";
  const description = "description";

  beforeEach(() => {
    program = new Program("input1", title, description);
    const input = new Input("input1", ["number"]);
    const op1 = new MovingAverage("ma1", 2);
    const output = new Output("out1", ["number"]);
    program.addOperator(input);
    program.addOperator(op1);
    program.addOperator(output);
    program.addConnection(input, op1);
    program.addConnection(op1, output);
    program.addOutput("out1", ["o1"]);
  });

  // Rest of the test cases remain the same...
  it("can create a new instance", () => {
    expect(program.title).toBe(title);
  });

  it("can be serialized", () => {
    const json = JSON.stringify(program);
    const parsedProgram = JSON.parse(json);
    expect(parsedProgram.operators.find((op: any) => op.id === "ma1")).toMatchObject({ id: "ma1", window_size: 2 });
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
    const rtbotRun = new RtBotRun(program, data, RtBotRunOutputFormat.EXTENDED);
    await rtbotRun.run();
    const result = rtbotRun.getOutputs();
    expect(result).toBeDefined();
    expect(result.length).toBe(data.length);
  });

  it("there has to be at least one Input operator in a program to be valid", () => {
    const input = program.operators.find((op) => op.type === "Input");
    expect(input).toBeDefined();
  });

  it("cannot add a connection if the first operator hasn't been added to the program", () => {
    const ma2 = new MovingAverage("ma2", 2);
    const ma3 = new MovingAverage("ma3", 2);
    const wrap = () => {
      program.addConnection(ma2, ma3);
    };
    expect(wrap).toThrow(/(.*)operator ma2 hasn't been added(.*)/);
  });

  it("cannot add a connection if the second operator hasn't been added to the program", () => {
    const ma2 = new MovingAverage("ma2", 2);
    program.addOperator(ma2);
    const ma3 = new MovingAverage("ma3", 2);
    const wrap = () => {
      program.addConnection(ma2, ma3);
    };
    expect(wrap).toThrow(/(.*)operator ma3 hasn't been added(.*)/);
  });

  it("cannot add a connection twice", () => {
    const ma2 = new MovingAverage("ma2", 2);
    program.addOperator(ma2);
    const ma3 = new MovingAverage("ma3", 2);
    program.addOperator(ma3);
    program.addConnection(ma2, ma3);
    const wrap = () => {
      program.addConnection(ma2, ma3);
    };
    expect(wrap).toThrow(/(.*)There is already a connection(.*)/);
  });

  it("can parse a valid json and create an instance of a Program", () => {
    const wrap = () => {
      const plain = JSON.parse(programJson);
      const program = Program.toInstance(plain);
      program.validate();
    };
    expect(wrap).not.toThrow();
  });
});

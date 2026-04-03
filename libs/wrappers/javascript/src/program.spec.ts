import { Program, MovingAverage, Input, Output, RtBot, RtBotRun, RtBotRunOutputFormat } from "@rtbot-dev/rtbot";
import bindings from "@rtbot-dev/wasm";

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

  it("can process vector rows as a single vector message per iteration", async () => {
    const vectorProgram = Program.toInstance({
      operators: [
        { id: "in1", type: "Input", portTypes: ["vector_number"] },
        { id: "out1", type: "Output", portTypes: ["vector_number"] },
      ],
      connections: [{ from: "in1", to: "out1", fromPort: "o1", toPort: "i1" }],
      entryOperator: "in1",
      output: { out1: ["o1"] },
    });

    const vectorData = [
      [1, 10, 20, 30],
      [2, 7, 8, 9],
    ];

    const run = new RtBotRun(vectorProgram, vectorData, RtBotRunOutputFormat.EXTENDED);
    await run.run();
    const outputs = run.getOutputs() as Array<{
      out: Record<string, Record<string, Array<{ time: number; value: number[] }>>>;
    }>;

    expect(outputs.length).toBe(2);
    expect(outputs[0].out.out1.o1[0].time).toBe(1);
    expect(outputs[0].out.out1.o1[0].value).toEqual([10, 20, 30]);
    expect(outputs[1].out.out1.o1[0].time).toBe(2);
    expect(outputs[1].out.out1.o1[0].value).toEqual([7, 8, 9]);
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

  it("can serialize and restore program data", async () => {
    const programId = "test_serialize_restore";
    const plainProgram = JSON.stringify(program.toPlain());

    const rtbot = RtBot.getInstance();

    // Create and feed data
    await rtbot.createProgram(programId, plainProgram);
    for (const [time, value] of data) {
      await rtbot.processDebug(programId, { i1: [{ time, value }] });
    }

    // Serialize state
    const state = await rtbot.serializeProgramData(programId);
    expect(state).toBeDefined();
    const parsed = JSON.parse(state);
    expect(parsed).toHaveProperty("ma1");

    // Delete and recreate, then restore
    await rtbot.deleteProgram(programId);
    await rtbot.createProgram(programId, plainProgram);
    await rtbot.restoreProgramDataFromJson(programId, state);

    // Process one more data point on restored program
    const restoredResult = await rtbot.processDebug(programId, { i1: [{ time: 7, value: 10.0 }] });

    // Do the same on a fresh program with all data
    const freshId = "test_serialize_fresh";
    await rtbot.createProgram(freshId, plainProgram);
    for (const [time, value] of data) {
      await rtbot.processDebug(freshId, { i1: [{ time, value }] });
    }
    const freshResult = await rtbot.processDebug(freshId, { i1: [{ time: 7, value: 10.0 }] });

    // Both should produce the same output
    expect(restoredResult).toEqual(freshResult);

    await rtbot.deleteProgram(programId);
    await rtbot.deleteProgram(freshId);
  });

  it("can call WASM addToMessageBuffer and beginVectorMessage with explicit id=0", async () => {
    const wasm = await bindings();

    const simpleProgram = JSON.stringify({
      operators: [
        { id: "in1", type: "Input", portTypes: ["number"] },
        { id: "out1", type: "Output", portTypes: ["number"] },
      ],
      connections: [{ from: "in1", to: "out1", fromPort: "o1", toPort: "i1" }],
      entryOperator: "in1",
      output: { out1: ["o1"] },
    });

    const created = wasm.createProgram("wasm_id_test", simpleProgram);
    expect(created).toBe("");

    // Call addToMessageBuffer directly with id=0 as 5th arg
    const added = wasm.addToMessageBuffer("wasm_id_test", "i1", 1, 42.0, 0);
    expect(added).toBe("1");

    const result = JSON.parse(wasm.processMessageBuffer("wasm_id_test"));
    expect(result["out1"]["o1"][0]["time"]).toBe(1);
    expect(result["out1"]["o1"][0]["value"]).toBeCloseTo(42.0);

    wasm.deleteProgram("wasm_id_test");

    // Test beginVectorMessage with id=0
    const vectorProgram = JSON.stringify({
      operators: [
        { id: "in1", type: "Input", portTypes: ["vector_number"] },
        { id: "out1", type: "Output", portTypes: ["vector_number"] },
      ],
      connections: [{ from: "in1", to: "out1", fromPort: "o1", toPort: "i1" }],
      entryOperator: "in1",
      output: { out1: ["o1"] },
    });

    wasm.createProgram("wasm_vec_id_test", vectorProgram);

    // Call beginVectorMessage directly with id=0 as 4th arg
    const started = wasm.beginVectorMessage("wasm_vec_id_test", "i1", 10, 0);
    expect(started).toBe("1");
    expect(wasm.pushVectorMessageValue("wasm_vec_id_test", "i1", 100.0)).toBe("1");
    expect(wasm.pushVectorMessageValue("wasm_vec_id_test", "i1", 200.0)).toBe("1");
    expect(wasm.endVectorMessage("wasm_vec_id_test", "i1")).toBe("1");

    const vecResult = JSON.parse(wasm.processMessageBuffer("wasm_vec_id_test"));
    expect(vecResult["out1"]["o1"][0]["time"]).toBe(10);
    expect(vecResult["out1"]["o1"][0]["value"]).toEqual([100.0, 200.0]);

    wasm.deleteProgram("wasm_vec_id_test");
  });

  it("can pass explicit id=0 to message buffer", async () => {
    const programId = "test_message_id";
    const plainProgram = JSON.stringify(program.toPlain());

    const rtbot = RtBot.getInstance();
    await rtbot.createProgram(programId, plainProgram);

    // MA window_size=2, need 2 messages before output appears
    await rtbot.processDebug(programId, { i1: [{ time: 1, value: 10.0, id: 0 }] });
    const result = await rtbot.processDebug(programId, { i1: [{ time: 2, value: 20.0, id: 0 }] });
    expect(result).toBeDefined();
    expect(result["out1"]).toBeDefined();

    await rtbot.deleteProgram(programId);
  });
});

import { Program, MovingAverage, Input, Output, RtBot, RtBotRun, RtBotRunOutputFormat } from "@rtbot-dev/rtbot";

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

  it("toInstance → toPlain roundtrip preserves deeply nested operator properties", () => {
    // This test documents the class-transformer lossy roundtrip for complex
    // operators like KeyedPipeline that contain a deeply nested `prototype`
    // field with its own operators, connections, and entry/output definitions.
    //
    // plainToInstance(Program, obj) creates generic Operator instances.
    // For simple properties (window_size: number), they survive the roundtrip.
    // For deeply nested objects (prototype: {operators: [...], ...}), they
    // may be corrupted or lost by class-transformer.
    const complexProgramJson = {
      entryOperator: "in1",
      operators: [
        { id: "in1", type: "Input", portTypes: ["vector_number"] },
        {
          id: "kp1",
          type: "KeyedPipeline",
          key_index: 0,
          prototype: {
            entryOperator: "proto_in",
            operators: [
              { id: "proto_in", type: "Input", portTypes: ["vector_number"] },
              { id: "ve1", type: "VectorExtract", index: 1 },
              { id: "ma1", type: "MovingAverage", window_size: 5 },
              { id: "proto_out", type: "Output", portTypes: ["number"] },
            ],
            connections: [
              { from: "proto_in", to: "ve1", fromPort: "o1", toPort: "i1" },
              { from: "ve1", to: "ma1", fromPort: "o1", toPort: "i1" },
              { from: "ma1", to: "proto_out", fromPort: "o1", toPort: "i1" },
            ],
            output: { proto_out: ["o1"] },
          },
        },
        { id: "out1", type: "Output", portTypes: ["vector_number"] },
      ],
      connections: [
        { from: "in1", to: "kp1", fromPort: "o1", toPort: "i1" },
        { from: "kp1", to: "out1", fromPort: "o1", toPort: "i1" },
      ],
      output: { out1: ["o1"] },
    };

    const instance = Program.toInstance(complexProgramJson as Record<string, unknown>);
    const roundtripped = instance.toPlain() as Record<string, unknown>;

    // Check that the top-level KeyedPipeline properties survive
    const originalKp = complexProgramJson.operators.find((op) => op.id === "kp1");
    const roundtrippedKp = (roundtripped.operators as any[])?.find((op: any) => op.id === "kp1");

    // key_index should survive (simple number property)
    expect(roundtrippedKp?.key_index).toBe(0);

    // The deeply nested prototype should survive the roundtrip
    expect(roundtrippedKp?.prototype).toBeDefined();
    expect(roundtrippedKp?.prototype?.operators).toBeDefined();
    expect(roundtrippedKp?.prototype?.operators?.length).toBe(
      originalKp!.prototype.operators.length,
    );

    // Check inner operator properties survive
    const innerMa = roundtrippedKp?.prototype?.operators?.find(
      (op: any) => op.id === "ma1",
    );
    expect(innerMa?.window_size).toBe(5);

    // Check connections survive
    expect(roundtrippedKp?.prototype?.connections?.length).toBe(
      originalKp!.prototype.connections.length,
    );

    // The overall JSON size should be preserved (not lose half the content)
    const originalSize = JSON.stringify(complexProgramJson).length;
    const roundtrippedSize = JSON.stringify(roundtripped).length;
    // Allow 20% size difference for formatting/metadata, but not 50%+ loss
    expect(roundtrippedSize).toBeGreaterThan(originalSize * 0.8);
  });

  it("RtBotRun constructed from JSON string produces correct output", async () => {
    // When RtBotRun receives a string, it does:
    //   Program.toInstance(JSON.parse(str)) → store as this.program
    //   run() → this.program.toPlain() → JSON.stringify → createProgram
    // If the roundtrip is lossy, operator properties like window_size are
    // dropped and the WASM program behaves incorrectly (or crashes).
    //
    // A simple MovingAverage(window_size=2) program should produce
    // the running average. If window_size is lost, the output will differ.
    const simpleProgram = JSON.stringify({
      operators: [
        { id: "in1", type: "Input", portTypes: ["number"] },
        { id: "ma1", type: "MovingAverage", window_size: 2 },
        { id: "out1", type: "Output", portTypes: ["number"] },
      ],
      connections: [
        { from: "in1", to: "ma1", fromPort: "o1", toPort: "i1" },
        { from: "ma1", to: "out1", fromPort: "o1", toPort: "i1" },
      ],
      entryOperator: "in1",
      output: { out1: ["o1"] },
    });

    const rtbotRun = new RtBotRun(
      simpleProgram,
      data,
      RtBotRunOutputFormat.EXTENDED,
    );
    await rtbotRun.run();
    const result = rtbotRun.getOutputs();
    expect(result).toBeDefined();
    expect(result.length).toBe(data.length);

    // Verify the moving average is actually computed with window_size=2.
    // data = [10, 20, 30, 25, 20, 15]
    // MA(2): [10, 15, 25, 27.5, 22.5, 17.5]
    const outputs = result as Array<{
      out?: Record<string, Record<string, Array<{ time: number; value: number }>>>;
    }>;
    // The second output should be the average of 10 and 20 = 15
    const secondOutput = outputs[1]?.out?.out1?.o1?.[0]?.value;
    expect(secondOutput).toBeCloseTo(15, 5);
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
});

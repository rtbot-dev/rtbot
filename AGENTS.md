# RtBot Repository

Real-time analytical streaming engine written in C++17. Processes streams of numerical data one message at a time through directed acyclic graphs (DAGs) of operators. Produces deterministic, causal, incremental rolling computations.

**Design principles**: deterministic (same input = same output, event-time only, no wall clocks), causal (only past data determines output, no lookahead), incremental (constant-time updates per message regardless of window size), declarative (behavior determined by operator topology expressed as JSON, not execution order).

## Critical: build rules

**Always use Bazel.** Never invoke bare build tools (cmake, make, g++, clang++, npm, npx, cargo, etc.) directly. All builds, tests, and dev servers go through Bazel.

## Build system

Bazel with bzlmod (`MODULE.bazel`). C++17 with clang. Output goes to `dist/` (configured in `.bazelrc`).

Supports Linux (clang), macOS (clang + libc++), Windows, and Emscripten (WASM).

### Key dependencies

- catch2 (testing), json-cpp (JSON), yaml-cpp, nlohmann/json (schema validation)
- lua + sol2 (scripting), pybind11 (Python bindings), emsdk 4.0.15 (WASM)
- protobuf 32.1, ftxui (TUI), cxxopts (CLI), quill (logging)
- aspect_rules_js/ts (JavaScript/TypeScript tooling), pnpm

### Common commands

```bash
bazel build //...                              # build everything
bazel test //...                               # run all tests
bazel build --stamp //libs/api:js              # stamped WASM npm package
bazel build //libs/api:rtbot-wasm              # WASM build
bazel build //libs/wrappers/python:rtbot_py    # python bindings
bazel run //docs/site:start                    # docs dev server
```

## Directory structure

```
rtbot/
├── libs/
│   ├── core/          # fundamental operator framework and core operators
│   ├── std/           # standard operators library (~40 operators)
│   ├── fuse/          # fused-expression bytecode + batched/SIMD evaluator
│   ├── finance/       # finance-specific operators (RSI)
│   ├── api/           # public API: Program, ProgramManager, WASM/JS bindings
│   ├── extension/     # extension mechanism (Lua scripting)
│   └── wrappers/      # language bindings (Python, JavaScript, Java)
├── apps/
│   └── benchmark/     # performance benchmarks
├── examples/          # example programs and notebooks
├── docs/              # docusaurus-based documentation site
├── tools/
│   ├── generator/     # TypeScript code generator
│   └── external/      # external dependency definitions
├── MODULE.bazel
└── .bazelrc
```

## Core architecture

### Messages and ports

Messages carry a timestamp (`int64_t`, event-time) and typed payload. Four data types:

| Type | C++ type | Description |
|------|----------|-------------|
| `NumberData` | `double` | single numeric value |
| `BooleanData` | `bool` | single boolean |
| `VectorNumberData` | `shared_ptr<vector<double>>` | shared-immutable numeric vector |
| `VectorBooleanData` | packed booleans | boolean vector |

Defined in `libs/core/include/rtbot/Message.h`.

Ports are typed (number, boolean, vector_number, vector_boolean). Three kinds: **data ports** (regular input/output), **control ports** (secondary input for control signals like segment keys), **output ports**. Type safety enforced at runtime on connection.

Message queues: fixed-size deque (MAX_SIZE_PER_PORT = 50,000). Thread-local object pool reduces heap allocation. Monotonically increasing timestamps enforced.

### Operator framework

Base class: `Operator` in `libs/core/include/rtbot/Operator.h`.

Key virtual methods:
- `type_name()` -- returns operator type string
- `process_data()` -- core computation logic
- `process_control()` -- optional control signal handling
- `collect_bytes()` / `restore()` -- serialization for state persistence

Execution model:
1. Operator receives messages via `receive_data()` / `receive_control()`
2. Runtime calls `execute()`: processes control messages first, then data messages
3. `propagate_outputs()` routes outputs to connected operators
4. Downstream operators execute recursively
5. Program collects outputs from mapped output operators

Connections enforce message type compatibility. Fast path optimization for single-connection operators (avoids cloning).

### Synchronization

Multi-input operators (Join, arithmetic) synchronize by matching timestamps: discard oldest messages until synchronization point found. Enables deterministic multi-stream aggregation.

Pipeline operators pair data messages with control messages by timestamp. Unpaired messages discarded asynchronously. On match: evaluate if key changed, emit accumulated outputs.

### State persistence

Operators serialize state to bytes (base64-encoded JSON). Deterministic replay: save state -> restore -> replay data -> identical outputs. Format includes timestamps, port counts, message queues, and operator-specific state.

## Operator catalog

### Core operators (`libs/core/`)

| Operator | Purpose |
|----------|---------|
| `Input` | multi-port entry / router into program (typed ports, monotonicity check) |
| `Output` | multi-port labelled exit (still supported; newer programs use `Collector`) |
| `Collector` | program-owned sink; accumulates outputs for external reading, no queue-size limit |
| `Buffer` | sliding window base class with Kahan sum and Welford variance |
| `Pipeline` | segment-scoped computation with control signal demarcation |
| `TriggerSet` | conditional multiplexing with output on trigger event |
| `Join` | synchronizes multiple inputs by timestamp |
| `Demultiplexer` / `Multiplexer` | route messages by key |
| `ReduceJoin` | specialized join reducing multiple inputs to one |

### Standard library (`libs/std/`, ~40 operators)

**Windowed aggregations**: MovingAverage, MovingSum, StandardDeviation, MovingKeyCount, MinMaxTracker, TopK, PeakDetector, WindowMinMax

**Filters and transforms**: Identity, Replace, TimeShift, TimestampExtract, Constant (number/boolean/cast variants), VectorCompose, VectorExtract, VectorProject

**Signal processing**: FiniteImpulseResponse (FIR), InfiniteImpulseResponse (IIR), CosineResampler, ResamplerConstant, ResamplerHermite, Difference, CumulativeSum

**Arithmetic**: ArithmeticScalar (Add, Scale, Power, Division), ArithmeticSync (multi-input Add, Multiply, Divide, Subtraction)

**Comparisons**: CompareScalar (GreaterThan, LessThan, EqualTo, etc.), CompareSync (multi-input)

**Boolean logic**: BooleanSync (LogicalAnd, Or, Xor, Nand, Nor, Implication), BooleanToNumber

**Stateful**: Count, Variable, KeyedVariable, Linear (weighted sum), Function (piecewise-linear interpolation)

**Advanced composites**: KeyedPipeline (prototype-based dynamic sub-graphs for keyed data, e.g. per-symbol aggregations)

### Fused expressions (`libs/fuse/`)

Compiled bytecode evaluator for fused arithmetic, windowed aggregates, and projection. Moved out of `libs/std/` to host:
- `FusedExpression` / `FusedExpressionVector` -- scalar and vector expression operators
- Packed 4-byte bytecode interpreter, scalar + SIMD (xsimd) batched evaluators
- Tier-1 windowed opcodes (moving aggregates) with mid-stream serialize/restore
- Gate opcode for predicate-based emission suppression
- `BurstAggregate` -- burst-oriented aggregation operator; overrides `Operator::receive_data_buffer` to skip per-row `Message` allocation

### Finance (`libs/finance/`)

RelativeStrengthIndex (RSI)

### Extension (`libs/extension/`)

LuaOperator -- custom user logic via Lua scripting (lua + sol2)

## Program JSON format

Programs are declared as JSON:

```json
{
  "title": "...",
  "apiVersion": "v1",
  "operators": [
    {"type": "Input", "id": "in1", "portTypes": ["number"]},
    {"type": "MovingAverage", "id": "ma", "window_size": 50},
    {"type": "Output", "id": "out1", "portTypes": ["number"]}
  ],
  "connections": [
    {"from": "in1", "to": "ma"},
    {"from": "ma", "to": "out1"}
  ],
  "entryOperator": "in1",
  "output": {"out1": ["o1"]},
  "prototypes": {}
}
```

The `prototypes` field defines reusable sub-graphs (parameterized templates) for operators like KeyedPipeline.

JSON deserialization factory: `libs/api/include/rtbot/OperatorJson.h`.

## APIs

### C++ (`libs/api/include/rtbot/Program.h`)

```cpp
Program program(json_string);
ProgramMsgBatch receive(const Message<NumberData>& msg, port_id = "i1");
ProgramMsgBatch receive_batch(const map<string, vector<BaseMessage>>& port_messages);
// Raw row-major double buffer ingress — skips Message allocation when the
// entry operator overrides Operator::receive_data_buffer (e.g. BurstAggregate).
ProgramMsgBatch receive_buffer(port_id, const double* data, size_t num_rows,
                               size_t num_cols, const timestamp_t* times);
ProgramMsgBatch receive_debug(const Message<NumberData>& msg);  // all intermediates
string serialize_data();
void restore_data_from_json(const string& json_state);
```

Outputs are accumulated in a Program-owned `Collector` sink (one data port per mapped program output) and returned by each `receive*` call as a `ProgramMsgBatch`.

`ProgramManager` for multi-program contexts: `create_program()`, `add_to_message_buffer()`, `process_message_buffer()`.

### Python (`libs/wrappers/python/rtbot.py`)

```python
from rtbot import Program, Run

program = Program(operators=[...], connections=[...], entryOperator="in1", output={"out1": ["o1"]})
run = Run(program, dataframe, port_mappings={"col": "port"})
result_df = run.exec()
```

Uses pybind11 C++ bindings. Pre-built `.so` at `libs/wrappers/python/rtbotc.so`.

### JavaScript/WASM (`libs/api/wasm/emscripten-bindings.cpp`)

Compiled via Emscripten. npm package: `@rtbot-dev/wasm`.

```javascript
module.createProgram(programId, jsonString);
module.addToMessageBuffer(programId, port, time, value);
module.processMessageBuffer(programId);
```

### Java (`libs/wrappers/java/`)

JNI bindings with JDK21.

## Testing

Framework: Catch2, BDD-style scenarios. Test dirs: `libs/{core,std,finance,api,extension}/test/`.

```bash
bazel test //...                                   # all tests
bazel test //libs/core/test                        # core tests
bazel test //libs/std/test                         # std operator tests
bazel test //libs/api/test                         # api/integration tests
```

TypeScript/JavaScript tests use Jest.

Benchmarks in `apps/benchmark/` -- PPG sensor data, SPY stock data, throughput and memory profiling.

## Documentation

Docusaurus site in `docs/site/`. Key pages:
- `docs/user-guide/core-concepts.md` -- message, signal, program iterations
- `docs/user-guide/operators.md` -- operator reference
- `docs/user-guide/design-principles.md` -- causal, deterministic, declarative
- `docs/user-guide/program-prototypes.md` -- reusable sub-graphs

Operator headers contain inline `.md` documentation files.

Dev server: `bazel run //docs/site:start`

## Performance notes

- Thread-local message pool reduces heap allocation from ~47% to negligible
- Fast path for single-connection operators (avoids cloning)
- Bitmask port propagation (vs. unordered_set)
- Shared-immutable vectors (`shared_ptr`) for zero-copy cloning
- Constant-time window operations via Kahan summation and Welford online variance
- Buffer template parameterized by feature flags (TRACK_SUM, TRACK_VARIANCE)

## Key files

| File | Purpose |
|------|---------|
| `libs/core/include/rtbot/Operator.h` | base operator class (includes `receive_data_buffer` / `receive_data_batch` hooks) |
| `libs/core/include/rtbot/Message.h` | message types and serialization |
| `libs/core/include/rtbot/Buffer.h` | sliding window base |
| `libs/core/include/rtbot/Pipeline.h` | segment-scoped computation |
| `libs/core/include/rtbot/Collector.h` | program-output sink operator |
| `libs/api/include/rtbot/Program.h` | program execution engine (Collector-backed sink) |
| `libs/api/include/rtbot/OperatorJson.h` | JSON deserialization factory |
| `libs/api/include/rtbot/Prototype.h` | prototype expansion |
| `libs/std/include/rtbot/std/KeyedPipeline.h` | dynamic keyed aggregation |
| `libs/std/include/rtbot/std/MovingAverage.h` | representative windowed operator |
| `libs/fuse/include/rtbot/fuse/FusedExpression.h` | bytecode expression operator |
| `libs/fuse/include/rtbot/fuse/BurstAggregate.h` | buffer-aware burst aggregation |
| `libs/wrappers/python/rtbot.py` | Python interface |
| `libs/api/wasm/emscripten-bindings.cpp` | JavaScript/WASM bindings |

## CI/CD

GitHub Actions. Trigger: push tags matching `v*.*.*`. Workflow:
1. Test (Ubuntu + macOS): `bazel test //...`
2. Build npm package: `bazel build --stamp //libs/api:js`
3. Build JSON schema: `bazel build //libs/api:jsonschema`
4. Publish: GitHub Release + npm publish (`@rtbot-dev/rtbot`, `@rtbot-dev/wasm`)

## License

BUSL-1.1 (converting to Apache 2.0 in 2029).

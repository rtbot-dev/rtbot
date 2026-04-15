---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \mathcal{F}({{numPorts}} \to {{numOutputs}})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
      examples: ["fused1"]
    numPorts:
      type: integer
      description: Number of scalar NUMBER input ports
      minimum: 1
      examples: [2]
    numOutputs:
      type: integer
      description: Number of output columns in the emitted vector
      minimum: 1
      examples: [1]
    bytecode:
      type: array
      description: Flat array of RPN opcodes encoding expression trees in postfix notation
      items:
        type: number
      examples: [[0, 0, 0, 1, 4, 20]]
    constants:
      type: array
      description: Compile-time constants referenced by CONST opcodes in the bytecode
      items:
        type: number
      examples: [[1.0, 2.0]]
    stateInit:
      type: array
      description: Initial values for persistent state slots used by stateful opcodes (CUMSUM, COUNT, MAX_AGG, MIN_AGG). Empty for pure expressions.
      items:
        type: number
      examples: [[0.0, 0.0, 0.0]]
  required: ["id", "numPorts", "numOutputs", "bytecode"]
---

# FusedExpression

Evaluates multiple arithmetic expressions over synchronized scalar inputs using an RPN bytecode interpreter. Replaces chains of arithmetic operators (between VectorExtract and VectorCompose) with a single operator, reducing per-message overhead.

Inherits from VectorCompose (which inherits from Join) for timestamp synchronization across input ports.

## Configuration

- `id`: Unique identifier for the operator
- `numPorts`: Number of scalar NUMBER input ports
- `numOutputs`: Number of output columns (one per expression)
- `bytecode`: Flat array of RPN opcodes (see Opcodes below)
- `constants`: Array of compile-time constants referenced by CONST opcodes (optional, defaults to empty)
- `stateInit`: Array of initial values for persistent state slots used by stateful opcodes (optional, defaults to empty)

```json
{
  "id": "fused1",
  "numPorts": 2,
  "numOutputs": 1,
  "bytecode": [0, 0, 0, 1, 4, 20],
  "constants": []
}
```

The above computes `input[0] * input[1]` (INPUT 0, INPUT 1, MUL, END).

## Ports

- Input Ports 0..(numPorts-1): NumberData (scalar values)
- Output Port 0: VectorNumberData (vector of numOutputs evaluated expressions)

## Opcodes

Each opcode is encoded as a double. Opcodes with arguments consume the next double.

| Code | Name  | Args | Stack Effect           |
| ---- | ----- | ---- | ---------------------- |
| 0    | INPUT | idx  | push inputs[idx]       |
| 1    | CONST | idx  | push constants[idx]    |
| 2    | ADD   | -    | pop b, a; push a+b     |
| 3    | SUB   | -    | pop b, a; push a-b     |
| 4    | MUL   | -    | pop b, a; push a*b     |
| 5    | DIV   | -    | pop b, a; push a/b     |
| 6    | POW   | -    | pop b, a; push pow(a,b)|
| 7    | ABS   | -    | pop a; push abs(a)     |
| 8    | SQRT  | -    | pop a; push sqrt(a)    |
| 9    | LOG   | -    | pop a; push ln(a)      |
| 10   | LOG10 | -    | pop a; push log10(a)   |
| 11   | EXP   | -    | pop a; push exp(a)     |
| 12   | SIN   | -    | pop a; push sin(a)     |
| 13   | COS   | -    | pop a; push cos(a)     |
| 14   | TAN   | -    | pop a; push tan(a)     |
| 15   | SIGN  | -    | pop a; push sign(a)    |
| 16   | FLOOR | -    | pop a; push floor(a)   |
| 17   | CEIL  | -    | pop a; push ceil(a)    |
| 18   | ROUND | -    | pop a; push round(a)   |
| 19   | NEG   | -    | pop a; push -a         |
| 20   | END   | -    | emit top as output     |
| 21   | CUMSUM     | idx  | pop a; Kahan-add to state[idx]; push state[idx] |
| 22   | COUNT      | idx  | state[idx] += 1; push state[idx] |
| 23   | MAX_AGG    | idx  | pop a; state[idx] = max(state[idx], a); push state[idx] |
| 24   | MIN_AGG    | idx  | pop a; state[idx] = min(state[idx], a); push state[idx] |
| 25   | STATE_LOAD | idx  | push state[idx] (read-only) |

The bytecode must contain exactly `numOutputs` END markers. Each END terminates one expression and resets the evaluation stack.

## Operation

1. Waits for all input ports to have messages with matching timestamps (Join synchronization)
2. Reads scalar values from each input port
3. Evaluates bytecode sequentially: each END marker emits the stack top as one output column
4. Assembles all output columns into a VectorNumberData message

### Stateful Opcodes

Opcodes 21-25 access persistent state slots that survive across messages. The `stateInit` array sets the initial value of each slot (e.g., `[0.0, -Infinity, Infinity]` for CUMSUM, MAX_AGG, MIN_AGG respectively). State is mutated in place on every evaluation and is never reset automatically.

### Example: two expressions over three inputs

Pass through input 0, and multiply inputs 1 and 2:

- numPorts: 3
- numOutputs: 2
- bytecode: INPUT 0, END, INPUT 1, INPUT 2, MUL, END
- Encoded: [0, 0, 20, 0, 1, 0, 2, 4, 20]

| Time | Port 0 | Port 1 | Port 2 | Output           |
| ---- | ------ | ------ | ------ | ---------------- |
| 1    | 42.0   | 150.0  | 200.0  | [42.0, 30000.0]  |
| 2    | 42.0   | 155.0  | 100.0  | [42.0, 15500.0]  |

## Error Handling

- Throws if numOutputs < 1 at construction
- Throws if bytecode END marker count does not match numOutputs
- Throws on unknown opcode during evaluation
- Throws on invalid message types

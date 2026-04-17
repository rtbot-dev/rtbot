---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \mathcal{F}_v(1 \to {{numOutputs}})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
      examples: ["fusedv1"]
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
  required: ["id", "numOutputs", "bytecode"]
---

# FusedExpressionVector

Evaluates multiple arithmetic expressions over a single VectorNumberData input using an RPN bytecode interpreter. This avoids pre-extracting scalar columns with separate VectorExtract operators.

## Configuration

- `id`: Unique identifier for the operator
- `numOutputs`: Number of output columns (one per expression)
- `bytecode`: Flat array of RPN opcodes (same opcode set as FusedExpression)
- `constants`: Array of compile-time constants referenced by CONST opcodes (optional, defaults to empty)
- `stateInit`: Array of initial values for persistent state slots used by stateful opcodes (optional, defaults to empty)

## Ports

- Input Port `i1`: VectorNumberData (full row vector)
- Output Port `o1`: VectorNumberData (vector of `numOutputs` evaluated expressions)

`INPUT k` in the bytecode reads column `k` directly from the incoming vector.

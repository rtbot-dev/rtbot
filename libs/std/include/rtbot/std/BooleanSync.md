---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
operators:
  LogicalAnd:
    latex:
      template: |
        \wedge
  LogicalOr:
    latex:
      template: |
        \vee
  LogicalXor:
    latex:
      template: |
        \veebar
  LogicalNand:
    latex:
      template: |
        \uparrow
  LogicalNor:
    latex:
      template: |
        \downarrow
  LogicalXnor:
    latex:
      template: |
        \leftrightarrow
  LogicalImplication:
    latex:
      template: |
        \rightarrow
jsonschemas:
  - type: object
    title: LogicalAnd
    properties:
      type:
        type: string
        enum: ["LogicalAnd"]
      id:
        type: string
        description: The id of the operator
      num_ports:
        type: integer
        minimum: 2
        default: 2
        description: "Number of input ports (default: 2)"
    required: ["id"]
  - type: object
    title: LogicalOr
    properties:
      type:
        type: string
        enum: ["LogicalOr"]
      id:
        type: string
        description: The id of the operator
      num_ports:
        type: integer
        minimum: 2
        default: 2
        description: "Number of input ports (default: 2)"
    required: ["id"]
  - type: object
    title: LogicalXor
    properties:
      type:
        type: string
        enum: ["LogicalXor"]
      id:
        type: string
        description: The id of the operator
      num_ports:
        type: integer
        minimum: 2
        default: 2
        description: "Number of input ports (default: 2)"
    required: ["id"]
  - type: object
    title: LogicalNand
    properties:
      type:
        type: string
        enum: ["LogicalNand"]
      id:
        type: string
        description: The id of the operator
      num_ports:
        type: integer
        minimum: 2
        default: 2
        description: "Number of input ports (default: 2)"
    required: ["id"]
  - type: object
    title: LogicalNor
    properties:
      type:
        type: string
        enum: ["LogicalNor"]
      id:
        type: string
        description: The id of the operator
      num_ports:
        type: integer
        minimum: 2
        default: 2
        description: "Number of input ports (default: 2)"
    required: ["id"]
  - type: object
    title: LogicalXnor
    properties:
      type:
        type: string
        enum: ["LogicalXnor"]
      id:
        type: string
        description: The id of the operator
      num_ports:
        type: integer
        minimum: 2
        default: 2
        description: "Number of input ports (default: 2)"
    required: ["id"]
  - type: object
    title: LogicalImplication
    properties:
      type:
        type: string
        enum: ["LogicalImplication"]
      id:
        type: string
        description: The id of the operator
      num_ports:
        type: integer
        minimum: 2
        default: 2
        description: "Number of input ports (default: 2)"
    required: ["id"]
---

# Boolean Synchronization Operators

A family of operators that perform logical operations on synchronized boolean streams with multiple inputs.

## Common Properties

All operators:

- Accept n input ports (default: 2) of type BooleanData
- Produce one output port (0) of type BooleanData
- Synchronize input messages based on timestamps
- Output messages only when all inputs have matching timestamps
- Inherit buffering behavior from ReduceJoin

## Available Operators

### LogicalAnd (∧)

Emits true only if all inputs are true. Initial value: true.

```
y(t_n) = x₀(t_n) ∧ x₁(t_n) ∧ ... ∧ xₙ(t_n)
```

### LogicalOr (∨)

Emits true if any input is true. Initial value: false.

```
y(t_n) = x₀(t_n) ∨ x₁(t_n) ∨ ... ∨ xₙ(t_n)
```

### LogicalXor (⊕)

Emits true if odd number of inputs are true. Initial value: false.

```
y(t_n) = x₀(t_n) ⊕ x₁(t_n) ⊕ ... ⊕ xₙ(t_n)
```

### LogicalNand (↑)

Emits false only if all inputs are true. Initial value: true.

```
y(t_n) = ¬(x₀(t_n) ∧ x₁(t_n) ∧ ... ∧ xₙ(t_n))
```

### LogicalNor (↓)

Emits true only if all inputs are false. Initial value: true.

```
y(t_n) = ¬(x₀(t_n) ∨ x₁(t_n) ∨ ... ∨ xₙ(t_n))
```

### LogicalXnor (↔)

Emits true if even number of inputs are true. Initial value: true.

```
y(t_n) = x₀(t_n) ↔ x₁(t_n) ↔ ... ↔ xₙ(t_n)
```

### LogicalImplication (→)

Chains implications: `a → b → c`. Initial value: true.
For two inputs, emits true unless first is true and second is false.

```
y(t_n) = x₀(t_n) → x₁(t_n) → ... → xₙ(t_n)
```

## Example Message Flow

Consider a LogicalAnd operator with three inputs:

| Time | Port 0 | Port 1 | Port 2 | Output |
| ---- | ------ | ------ | ------ | ------ |
| 1    | true   | -      | true   | -      |
| 2    | -      | false  | true   | -      |
| 3    | true   | true   | true   | true   |
| 4    | true   | false  | true   | false  |
| 5    | false  | -      | true   | -      |
| 6    | true   | true   | true   | true   |

Note that output is only produced when all three inputs have messages with matching timestamps (t=3, t=4, and t=6).

## Usage

```cpp
// Create operators using factory functions
auto and_op = make_logical_and("and1", 3);  // 3 inputs
auto or_op = make_logical_or("or1");        // default 2 inputs
auto xor_op = make_logical_xor("xor1", 4);  // 4 inputs

// Example usage with three inputs
and_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0);
and_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 1);
and_op->receive_data(create_message<BooleanData>(1, BooleanData{false}), 2);
and_op->execute();  // Will output false
```

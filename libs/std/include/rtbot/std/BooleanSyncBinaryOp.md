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
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
  required: ["id"]
---

# Boolean Synchronization Binary Operators

A family of operators that perform logical operations on synchronized boolean streams.

## Common Properties

All operators:

- Accept two input ports (0, 1) of type BooleanData
- Produce one output port (0) of type BooleanData
- Synchronize input messages based on timestamps
- Output messages only when both inputs have matching timestamps
- Inherit buffering behavior from BinaryJoin

## Available Operators

### LogicalAnd (∧)

Emits true only if both inputs are true.

```
y(t_n) = x₀(t_n) ∧ x₁(t_n)
```

### LogicalOr (∨)

Emits true if either input is true.

```
y(t_n) = x₀(t_n) ∨ x₁(t_n)
```

### LogicalXor (⊕)

Emits true if inputs are different.

```
y(t_n) = x₀(t_n) ⊕ x₁(t_n)
```

### LogicalNand (↑)

Emits false only if both inputs are true.

```
y(t_n) = ¬(x₀(t_n) ∧ x₁(t_n))
```

### LogicalNor (↓)

Emits true only if both inputs are false.

```
y(t_n) = ¬(x₀(t_n) ∨ x₁(t_n))
```

### LogicalXnor (↔)

Emits true if inputs are the same (logical equivalence).

```
y(t_n) = x₀(t_n) ↔ x₁(t_n)
```

### LogicalImplication (→)

Emits true unless first input is true and second is false.

```
y(t_n) = x₀(t_n) → x₁(t_n)
```

## Truth Tables

### Basic Operations

| x₀  | x₁  | AND | OR  | XOR | XNOR |
| --- | --- | --- | --- | --- | ---- |
| 0   | 0   | 0   | 0   | 0   | 1    |
| 0   | 1   | 0   | 1   | 1   | 0    |
| 1   | 0   | 0   | 1   | 1   | 0    |
| 1   | 1   | 1   | 1   | 0   | 1    |

### Derived Operations

| x₀  | x₁  | NAND | NOR | IMPL |
| --- | --- | ---- | --- | ---- |
| 0   | 0   | 1    | 1   | 1    |
| 0   | 1   | 1    | 0   | 1    |
| 1   | 0   | 1    | 0   | 0    |
| 1   | 1   | 0    | 0   | 1    |

## Example Message Flow

Consider a LogicalAnd operator with the following input messages:

| Time | Port 0 | Port 1 | Output |
| ---- | ------ | ------ | ------ |
| 1    | true   | -      | -      |
| 2    | -      | false  | -      |
| 3    | true   | true   | true   |
| 4    | true   | false  | false  |
| 5    | false  | -      | -      |
| 6    | -      | false  | -      |
| 7    | true   | true   | true   |

Note that output is only produced when both inputs have messages with matching timestamps (t=3, t=4, and t=7).

## Usage

```cpp
// Create operators using factory functions
auto and_op = make_logical_and("and1");
auto or_op = make_logical_or("or1");
auto xor_op = make_logical_xor("xor1");

// Example usage
and_op->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0);
and_op->receive_data(create_message<BooleanData>(1, BooleanData{false}), 1);
and_op->execute();  // Will output false
```

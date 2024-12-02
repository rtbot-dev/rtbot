---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
operators:
  Addition:
    latex:
      template: |
        +
  Subtraction:
    latex:
      template: |
        -
  Multiplication:
    latex:
      template: |
        ×
  Division:
    latex:
      template: |
        ÷
jsonschemas:
  - type: object
    title: Addition
    properties:
      type:
        type: string
        enum: ["Addition"]
      id:
        type: string
        description: The id of the operator
    required: ["id"]
  - type: object
    title: Subtraction
    properties:
      type:
        type: string
        enum: ["Subtraction"]
      id:
        type: string
        description: The id of the operator
    required: ["id"]
  - type: object
    title: Multiplication
    properties:
      type:
        type: string
        enum: ["Multiplication"]
      id:
        type: string
        description: The id of the operator
    required: ["id"]
  - type: object
    title: Division
    properties:
      type:
        type: string
        enum: ["Division"]
      id:
        type: string
        description: The id of the operator
    required: ["id"]
---

# Mathematical Synchronization Binary Operators

A family of operators that perform basic arithmetic operations on synchronized numeric streams.

## Common Properties

All operators:

- Accept two input ports (0, 1)
- Produce one output port (0)
- Synchronize input messages based on timestamps
- Output messages only when both inputs have matching timestamps
- Inherit buffering behavior from BinaryJoin

## Available Operators

### Addition

Emits the sum of synchronized values.

```
y(t_n) = x₀(t_n) + x₁(t_n)
```

### Subtraction

Emits the difference of synchronized values (first input minus second).

```
y(t_n) = x₀(t_n) - x₁(t_n)
```

### Multiplication

Emits the product of synchronized values.

```
y(t_n) = x₀(t_n) × x₁(t_n)
```

### Division

Emits the quotient of synchronized values (first input divided by second).
Returns no output if the denominator is zero.

```
y(t_n) = x₀(t_n) ÷ x₁(t_n)  if x₁(t_n) ≠ 0
```

## Example Message Flow

Consider an Addition operator with the following input messages:

| Time | Port 0 | Port 1 | Output |
| ---- | ------ | ------ | ------ |
| 1    | 10.0   | -      | -      |
| 2    | -      | 5.0    | -      |
| 3    | 15.0   | 15.0   | 30.0   |
| 5    | 20.0   | -      | -      |
| 6    | -      | 10.0   | -      |
| 7    | 25.0   | 25.0   | 50.0   |

Note that output is only produced when both inputs have messages with matching timestamps (t=3 and t=7).

## Usage

```cpp
// Create operators using factory functions
auto add = make_addition("add1");
auto sub = make_subtraction("sub1");
auto mul = make_multiplication("mul1");
auto div = make_division("div1");

// Connect and use like any other operator
add->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
add->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
add->execute();
```

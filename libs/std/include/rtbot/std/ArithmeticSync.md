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
      num_ports:
        type: integer
        minimum: 2
        description: Number of input ports (default 2)
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
      num_ports:
        type: integer
        minimum: 2
        default: 2
        description: Number of input ports (fixed at 2)
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
      num_ports:
        type: integer
        minimum: 2
        description: Number of input ports (default 2)
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
      num_ports:
        type: integer
        minimum: 2
        default: 2
        description: Number of input ports (fixed at 2)
    required: ["id"]
---

# Mathematical Synchronization Operators

A family of operators that perform arithmetic operations on synchronized numeric streams.

## Common Properties

All operators:

- Accept two or more input ports (0 to n-1)
- Produce one output port (0)
- Synchronize input messages based on timestamps
- Output messages only when all inputs have matching timestamps
- Inherit buffering behavior from ReduceJoin

## Available Operators

### Addition

Computes the sum of all synchronized values.

Initial value: 0.0

For n inputs:

```
y(t_n) = x₀(t_n) + x₁(t_n) + ... + xₙ₋₁(t_n)
```

### Multiplication

Computes the product of all synchronized values.

Initial value: 1.0

For n inputs:

```
y(t_n) = x₀(t_n) × x₁(t_n) × ... × xₙ₋₁(t_n)
```

### Subtraction

Takes the first input and subtracts all subsequent inputs.

For n inputs:

```
y(t_n) = x₀(t_n) - (x₁(t_n) + x₂(t_n) + ... + xₙ₋₁(t_n))
```

With 2 inputs, reduces to standard subtraction:

```
y(t_n) = x₀(t_n) - x₁(t_n)
```

### Division

Takes the first input and divides by the product of all subsequent inputs.
Returns no output if the product of denominators is zero.

For n inputs:

```
y(t_n) = x₀(t_n) ÷ (x₁(t_n) × x₂(t_n) × ... × xₙ₋₁(t_n))
```

With 2 inputs, reduces to standard division:

```
y(t_n) = x₀(t_n) ÷ x₁(t_n)  if x₁(t_n) ≠ 0
```

## Example Message Flows

Addition with 3 inputs:

```
Time | Port 0 | Port 1 | Port 2 | Output
-----|--------|--------|--------|--------
1    | 10.0   | -      | 5.0    | -
2    | -      | 5.0    | -      | -
3    | 15.0   | 15.0   | 15.0   | 45.0
5    | 20.0   | -      | 10.0   | -
7    | 25.0   | 25.0   | 25.0   | 75.0
```

### Multiplication with 3 inputs:

```
Time | Port 0 | Port 1 | Port 2 | Output
-----|--------|--------|--------|--------
1    | 2.0    | -      | 3.0    | -
2    | -      | 4.0    | -      | -
3    | 2.0    | 2.0    | 2.0    | 8.0
4    | 3.0    | 3.0    | 3.0    | 27.0
```

### Subtraction with 3 inputs:

```
Time | Port 0 | Port 1 | Port 2 | Output
-----|--------|--------|--------|--------
1    | 100.0  | 20.0   | 30.0   | 50.0   # 100 - (20 + 30)
2    | 50.0   | 10.0   | 15.0   | 25.0   # 50 - (10 + 15)
```

### Division with 3 inputs:

```
Time | Port 0 | Port 1 | Port 2 | Output
-----|--------|--------|--------|--------
1    | 100.0  | 2.0    | 5.0    | 10.0   # 100 ÷ (2 × 5)
2    | 200.0  | 4.0    | 5.0    | 10.0   # 200 ÷ (4 × 5)
```

## Usage

```cpp
// Create operators using factory functions
auto add = make_addition("add1", 3);           // 3 inputs
auto mul = make_multiplication("mul1", 4);      // 4 inputs
auto sub = make_subtraction("sub1", 3);        // x₀ - (x₁ + x₂)
auto div = make_division("div1", 3);           // x₀ ÷ (x₁ × x₂)
```

## Implementation Notes

1. Addition and Multiplication are commutative - order of inputs doesn't matter.

2. Subtraction treats first input (x₀) as minuend and subtracts sum of all other inputs.

3. Division treats first input (x₀) as numerator and divides by product of all other inputs.

4. All operators maintain backward compatibility with previous binary behavior when used with 2 inputs.

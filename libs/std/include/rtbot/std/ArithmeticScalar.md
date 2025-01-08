---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
operators:
  Add:
    latex:
      template: |
        x + {{value}}
  Scale:
    latex:
      template: |
        {{value}}x
  Power:
    latex:
      template: |
        x^{{value}}
  Sin:
    latex:
      template: |
        sin(x)
  Cos:
    latex:
      template: |
        cos(x)
  Tan:
    latex:
      template: |
        tan(x)
  Exp:
    latex:
      template: |
        e^x
  Log:
    latex:
      template: |
        ln(x)
  Log10:
    latex:
      template: |
        log₁₀(x)
  Abs:
    latex:
      template: |
        |x|
  Sign:
    latex:
      template: |
        sgn(x)
  Floor:
    latex:
      template: |
        ⌊x⌋
  Ceil:
    latex:
      template: |
        ⌈x⌉
  Round:
    latex:
      template: |
        round(x)
jsonschemas:
  - type: object
    title: Add
    properties:
      type:
        type: string
        enum: ["Add"]
      id:
        type: string
        description: The id of the operator
      value:
        type: number
        description: The value to add to the input
    required: ["id", "value"]
  - type: object
    title: Scale
    properties:
      type:
        type: string
        enum: ["Scale"]
      id:
        type: string
        description: The id of the operator
      value:
        type: number
        description: The value to multiply the input by
    required: ["id", "value"]
  - type: object
    title: Power
    properties:
      type:
        type: string
        enum: ["Power"]
      id:
        type: string
        description: The id of the operator
      value:
        type: number
        description: The exponent value
    required: ["id", "value"]
  - type: object
    title: Sin
    properties:
      type:
        type: string
        enum: ["Sin"]
      id:
        type: string
        description: The id of the operator
    required: ["id"]
  - type: object
    title: Cos
    properties:
      type:
        type: string
        enum: ["Cos"]
      id:
        type: string
        description: The id of the operator
    required: ["id"]
  - type: object
    title: Tan
    properties:
      type:
        type: string
        enum: ["Tan"]
      id:
        type: string
        description: The id of the operator
    required: ["id"]
  - type: object
    title: Exp
    properties:
      type:
        type: string
        enum: ["Exp"]
      id:
        type: string
        description: The id of the operator
    required: ["id"]
  - type: object
    title: Log
    properties:
      type:
        type: string
        enum: ["Log"]
      id:
        type: string
        description: The id of the operator
    required: ["id"]
  - type: object
    title: Log10
    properties:
      type:
        type: string
        enum: ["Log10"]
      id:
        type: string
        description: The id of the operator
    required: ["id"]
  - type: object
    title: Abs
    properties:
      type:
        type: string
        enum: ["Abs"]
      id:
        type: string
        description: The id of the operator
    required: ["id"]
  - type: object
    title: Sign
    properties:
      type:
        type: string
        enum: ["Sign"]
      id:
        type: string
        description: The id of the operator
    required: ["id"]
  - type: object
    title: Floor
    properties:
      type:
        type: string
        enum: ["Floor"]
      id:
        type: string
        description: The id of the operator
    required: ["id"]
  - type: object
    title: Ceil
    properties:
      type:
        type: string
        enum: ["Ceil"]
      id:
        type: string
        description: The id of the operator
    required: ["id"]
  - type: object
    title: Round
    properties:
      type:
        type: string
        enum: ["Round"]
      id:
        type: string
        description: The id of the operator
    required: ["id"]
---

# Arithmetic Scalar Operators

The ArithmeticScalar class serves as a base for operators that perform mathematical operations on a single numeric input stream. Each derived class implements a specific mathematical function that is applied to each input value.

## Basic Operation

All ArithmeticScalar operators:

- Have one input port (port 0) for numeric data
- Have one output port (port 0) for numeric results
- Process messages immediately (no buffering)
- Preserve message timestamps
- Apply their mathematical operation to each input value

## Available Operators

### Arithmetic Operators

1. **Add** (`y = x + C`)

   - Additional parameter: `value` (constant to add)

   ```json
   {
     "id": "add1",
     "type": "Add",
     "value": 5.0
   }
   ```

2. **Scale** (`y = C × x`)

   - Additional parameter: `value` (scaling factor)

   ```json
   {
     "id": "scale1",
     "type": "Scale",
     "value": 2.0
   }
   ```

3. **Power** (`y = x^C`)
   - Additional parameter: `value` (exponent)
   ```json
   {
     "id": "pow1",
     "type": "Power",
     "value": 2.0
   }
   ```

### Trigonometric Functions

4. **Sin** (`y = sin(x)`)

   ```json
   {
     "id": "sin1",
     "type": "Sin"
   }
   ```

5. **Cos** (`y = cos(x)`)

   ```json
   {
     "id": "cos1",
     "type": "Cos"
   }
   ```

6. **Tan** (`y = tan(x)`)
   ```json
   {
     "id": "tan1",
     "type": "Tan"
   }
   ```

### Exponential and Logarithmic Functions

7. **Exp** (`y = e^x`)

   ```json
   {
     "id": "exp1",
     "type": "Exp"
   }
   ```

8. **Log** (`y = ln(x)`)

   ```json
   {
     "id": "log1",
     "type": "Log"
   }
   ```

9. **Log10** (`y = log₁₀(x)`)
   ```json
   {
     "id": "log10_1",
     "type": "Log10"
   }
   ```

### Absolute Value and Sign Functions

10. **Abs** (`y = |x|`)

    ```json
    {
      "id": "abs1",
      "type": "Abs"
    }
    ```

11. **Sign** (`y = sign(x)`)
    ```json
    {
      "id": "sign1",
      "type": "Sign"
    }
    ```

### Rounding Functions

12. **Floor** (`y = ⌊x⌋`)

    ```json
    {
      "id": "floor1",
      "type": "Floor"
    }
    ```

13. **Ceil** (`y = ⌈x⌉`)

    ```json
    {
      "id": "ceil1",
      "type": "Ceil"
    }
    ```

14. **Round** (`y = round(x)`)
    ```json
    {
      "id": "round1",
      "type": "Round"
    }
    ```

## Example Usage

### Message Flow Example

Consider an Add operator with value = 2.0:

| Time | Input | Output |
| ---- | ----- | ------ |
| 1    | 1.0   | 3.0    |
| 2    | 2.5   | 4.5    |
| 4    | -1.0  | 1.0    |
| 5    | 3.0   | 5.0    |

Note how:

- Time gaps are preserved (no message at t=3)
- Output timestamps match input timestamps
- The constant value (2.0) is added to each input

### Code Example

```cpp
// Create an Add operator
auto add = make_add("add1", 2.0);

// Create a Sine operator
auto sine = make_sin("sin1");

// Process some messages
add->receive_data(create_message<NumberData>(1, NumberData{1.0}), 0);
add->execute();

sine->receive_data(create_message<NumberData>(1, NumberData{M_PI/2}), 0);
sine->execute();
```

## Error Handling

Operators will throw exceptions for:

- Invalid message types
- Invalid input port indices
- Mathematical errors (e.g., log of negative numbers)

## Implementation Notes

1. **Stateless Operation**

   - No buffering of messages
   - Each output depends only on current input
   - Immediate processing

2. **Performance**

   - O(1) processing per message
   - Constant memory usage
   - No allocation during normal operation

3. **Numerical Considerations**
   - Uses IEEE 754 floating-point arithmetic
   - Handles special values (NaN, Inf) according to standard rules
   - Preserves numeric precision of underlying math functions

## Factory Functions

Each operator type has an associated factory function:

```cpp
auto add = make_add("add1", 2.0);
auto sin = make_sin("sin1");
auto exp = make_exp("exp1");
// etc.
```

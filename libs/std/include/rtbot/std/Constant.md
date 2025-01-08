---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \equiv {{value}}
jsonschemas:
  - type: object
    properties:
      type:
        enum: [ConstantNumber]
      id:
        type: string
        description: The id of the operator
      value:
        type: number
        examples:
          - 3.14
          - 2.718
        description: The constant value to emit for each input message
    required: ["id", "value"]
  - type: object
    properties:
      type:
        enum: [ConstantBoolean]
      id:
        type: string
        description: The id of the operator
      value:
        type: boolean
        examples:
          - true
          - false
        description: The constant value to emit for each input message
    required: ["id", "value"]
  - type: object
    properties:
      type:
        enum: [ConstantNumberToBoolean]
      id:
        type: string
        description: The id of the operator
      value:
        type: boolean
        examples:
          - true
          - false
        description: The constant value to emit for each input message
    required: ["id", "value"]
  - type: object
    properties:
      type:
        enum: [ConstantBooleanToNumber]
      id:
        type: string
        description: The id of the operator
      value:
        type: number
        examples:
          - 3.14
          - 2.718
        description: The constant value to emit for each input message
    required: ["id", "value"]
---

# Constant

The Constant operator emits a fixed value while preserving the timing of input messages. For each input message received, it outputs a message with the same timestamp but replaces the value with the configured constant.

## Ports

- Input Port 0: Accepts messages of type T
- Output Port 0: Emits messages of type T with constant value

## Operation

The operator maintains no internal state and processes messages immediately:

1. Receives an input message
2. Creates a new message with:
   - Same timestamp as input
   - Value field set to configured constant
3. Emits the new message

### Example Message Flow

| Time | Input (Port 0) | Output (Port 0) |
| ---- | -------------- | --------------- |
| 1    | 10.0           | 42.0            |
| 2    | 15.0           | 42.0            |
| 5    | 25.0           | 42.0            |
| 7    | 30.0           | 42.0            |

## Features

- Zero buffering - processes messages immediately
- Preserves message timing
- Type-safe operation
- Configurable for any data type
- Constant throughput (1:1 input to output)

## Implementation

```cpp
// Create operator with constant value 42.0
auto constant = make_number_constant("const1", 42.0);

// Process some values (will all be converted to 42.0)
constant->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
constant->execute();
```

## Error Handling

The operator throws exceptions for:

- Type mismatches on input
- Invalid port indices
- Message conversion failures

## Performance Characteristics

- O(1) processing per message
- No memory growth
- Zero-copy message handling where possible
- Immediate message processing

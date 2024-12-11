---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \sum
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
  required: ["id"]
---

# CumulativeSum

A streaming operator that maintains a running sum of all input values, emitting the cumulative total for each received message.

## Configuration

```json
{
  "id": "sum1"
}
```

## Ports

- Input Port 0: Accepts NumberData messages
- Output Port 0: Emits NumberData messages containing running sum

## Behavior

The operator:

1. Maintains a running sum of all input values
2. For each input message, outputs a message with:
   - Same timestamp as input
   - Value equal to current cumulative sum
3. Processes messages in arrival order
4. Maintains state between executions

Example sequence:

| Time | Input Value | Output (Running Sum) |
| ---- | ----------- | -------------------- |
| 1    | 10.0        | 10.0                 |
| 5    | 20.0        | 30.0                 |
| 10   | 15.0        | 45.0                 |
| 20   | 5.0         | 50.0                 |

## Implementation Details

- Uses double precision for sum calculations
- Supports serialization/restoration of state
- O(1) memory usage - only stores running sum
- O(1) processing time per message
- Thread-safe within single execution context

## Error Handling

Throws exceptions for:

- Invalid message types
- Type mismatches on input port

## Use Cases

Ideal for:

- Running totals
- Cumulative metrics
- Aggregate calculations
- Financial summations
- Signal integration

## Code Example

```cpp
// Create operator
auto sum = std::make_unique<CumulativeSum>("sum1");

// Process some values
sum->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
sum->receive_data(create_message<NumberData>(2, NumberData{20.0}), 0);
sum->execute();

// Access current sum
double total = sum->get_sum();  // Returns 30.0
```

---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \mathbb{B} \to \mathbb{R}
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
      examples: ["b2n1"]
  required: ["id"]
---

# BooleanToNumber

The BooleanToNumber operator converts BooleanData messages to NumberData messages. Each boolean value is mapped to a numeric equivalent: `true` becomes `1.0` and `false` becomes `0.0`. Timestamps are preserved.

## Configuration

Requires only an operator ID.

## Ports

- Input Port 0: Accepts BooleanData messages
- Output Port 0: Emits NumberData messages

## Example Operation

| Time | Input Port 0 (Boolean) | Output Port 0 (Number) |
| ---- | ---------------------- | ---------------------- |
| 1    | true                   | 1.0                    |
| 2    | false                  | 0.0                    |
| 3    | true                   | 1.0                    |
| 5    | false                  | 0.0                    |
| 7    | true                   | 1.0                    |

## Behavior

The BooleanToNumber operator:

- Maintains message order
- Preserves timestamps
- Does not buffer messages
- Has constant 1:1 throughput
- Maps `true` to `1.0` and `false` to `0.0`

Mathematical representation:
$$y(t_n) = \begin{cases} 1.0 & \text{if } x(t_n) = \text{true} \\ 0.0 & \text{if } x(t_n) = \text{false} \end{cases}$$

## Implementation

```cpp
// Create operator
auto b2n = make_boolean_to_number("b2n1");

// Process boolean values
b2n->receive_data(create_message<BooleanData>(1, BooleanData{true}), 0);
b2n->receive_data(create_message<BooleanData>(2, BooleanData{false}), 0);
b2n->execute();

// Output will contain NumberData messages: 1.0 at time 1, 0.0 at time 2
```

## Error Handling

Throws std::runtime_error if:

- Receiving message with incorrect type
- Receiving message on invalid port

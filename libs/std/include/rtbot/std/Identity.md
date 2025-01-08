---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \mathbb{1}
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
      examples: ["id1"]
  required: ["id"]
---

# Identity

The Identity operator forwards all input messages without modification, maintaining their timestamp and value. Each message received on input port 0 is immediately forwarded to output port 0.

## Configuration

Requires only an operator ID.

## Ports

- Input Port 0: Accepts NumberData messages
- Output Port 0: Emits NumberData messages

## Example Operation

| Time | Input Port 0 | Output Port 0 |
| ---- | ------------ | ------------- |
| 1    | 42.0         | 42.0          |
| 2    | -            | -             |
| 3    | 15.7         | 15.7          |
| 4    | -            | -             |
| 5    | 33.1         | 33.1          |

## Behavior

The Identity operator:

- Maintains message order
- Preserves timestamps
- Does not buffer messages
- Has constant 1:1 throughput
- Performs no data transformation

Mathematical representation:
$$y(t_n) = x(t_n)$$

## Error Handling

Throws std::runtime_error if:

- Receiving message with incorrect type
- Receiving message on invalid port

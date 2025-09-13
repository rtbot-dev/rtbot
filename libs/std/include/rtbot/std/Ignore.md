---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \mathbb{1} if count have been ignored
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
      examples: ["id1"]
    count:
      type: number
      description: The count to ignore
      examples: [40]
  required: ["id", "count"]
---

# Ignore

The Ignore operator forwards all input messages without modification once it reaches the indicated count to ignore, maintaining their timestamp and value. Each message received on input port 0 is immediately forwarded to output port 0.

## Configuration

Requires an operator ID.
Requires a count to ignore.

## Ports

- Input Port 0: Accepts NumberData messages
- Output Port 0: Emits NumberData messages
- Control Port 0: To reset the amount ignore back to zero

## Example Operation

| Time | Input Port 0 | Output Port 0 | Ignored | Count |
| ---- | ------------ | ------------- | ------- | ----- |
| 1    | 42.0         | -             | 1       | 2     |
| 2    | -            | -             | 1       | 2     |
| 3    | 15.7         | -             | 2       | 2     |
| 4    | -            | -             | 2       | 2     |
| 5    | 33.1         | 33.1          | 2       | 2     |
| 6    | 80.1         | 80.1          | 2       | 2     |

## Behavior

The Identity operator:

- Maintains message order
- Preserves timestamps
- Does not buffer messages
- Has constant 1:1 throughput when the amount driven by count has been ignored
- Performs no data transformation

Mathematical representation:
$$y(t_n) = x(t_n)$$ when the amount driven by count has been ignored

## Error Handling

Throws std::runtime_error if:

- Receiving message with incorrect type
- Receiving message on invalid port

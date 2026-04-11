---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      t
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
      examples: ["ts_0"]
  required: ["id"]
---

# TimestampExtract

Extracts the message timestamp as a scalar NumberData value.

## Configuration

- `id`: Unique identifier for the operator

```json
{
  "id": "ts_0"
}
```

## Ports

- Input Port 0: VectorNumberData
- Output Port 0: NumberData (the message timestamp cast to double)

## Operation

For each input vector message, outputs `static_cast<double>(msg->time)` as a scalar NumberData message with the same timestamp.

| Time | Input (vector)      | Output (scalar) |
| ---- | ------------------- | --------------- |
| 1000 | [10.0, 20.0, 30.0] | 1000.0          |
| 2000 | [40.0, 50.0, 60.0] | 2000.0          |

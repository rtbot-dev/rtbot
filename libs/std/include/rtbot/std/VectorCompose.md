---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \oplus({{numPorts}})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
      examples: ["comp1"]
    numPorts:
      type: integer
      description: Number of input ports to concatenate
      minimum: 1
      examples: [2]
  required: ["id", "numPorts"]
---

# VectorCompose

Concatenates multiple VectorNumberData inputs into a single output vector. Synchronizes all input ports by timestamp before combining.

## Configuration

- `id`: Unique identifier for the operator
- `numPorts`: Number of input ports (must be >= 1)

```json
{
  "id": "comp1",
  "numPorts": 3
}
```

## Ports

- Input Ports 0..(numPorts-1): VectorNumberData
- Output Port 0: VectorNumberData (concatenation of all inputs)

## Operation

Waits for all input ports to have messages with matching timestamps, then concatenates all input vectors in port order.

| Time | Port 0 (vector) | Port 1 (vector) | Output              |
| ---- | ---------------- | ---------------- | ------------------- |
| 1    | [10.0]           | [20.0, 30.0]     | [10.0, 20.0, 30.0] |
| 2    | [40.0]           | [50.0, 60.0]     | [40.0, 50.0, 60.0] |

## Error Handling

- Throws if numPorts < 1 at construction
- Throws if receiving invalid message types
- Drops messages on ports that arrive with timestamps earlier than other ports (synchronization)

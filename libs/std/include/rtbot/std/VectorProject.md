---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \pi_{indices}
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
      examples: ["proj1"]
    indices:
      type: array
      items:
        type: integer
        minimum: 0
      description: List of indices to select from the input vector
      examples: [[0, 2]]
  required: ["id", "indices"]
---

# VectorProject

Selects a subset of elements from an input vector by their indices, producing a new vector.

## Configuration

- `id`: Unique identifier for the operator
- `indices`: Array of zero-based indices to extract (must be non-empty, all non-negative)

```json
{
  "id": "proj1",
  "indices": [0, 2, 3]
}
```

## Ports

- Input Port 0: VectorNumberData
- Output Port 0: VectorNumberData (subset of the input vector)

## Operation

For each input vector, outputs a new vector containing only the elements at the specified indices, in the order specified.

| Time | Input (vector)         | Output (indices=[1,2]) |
| ---- | ---------------------- | ---------------------- |
| 1    | [10.0, 20.0, 30.0]    | [20.0, 30.0]          |
| 2    | [40.0, 50.0, 60.0]    | [50.0, 60.0]          |

## Error Handling

- Throws if indices array is empty at construction
- Throws if any index is negative at construction
- Throws at runtime if any index is out of bounds for the input vector

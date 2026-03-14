---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      v[{{index}}]
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
      examples: ["ext1"]
    index:
      type: integer
      description: The index of the element to extract from the input vector
      minimum: 0
      examples: [0]
  required: ["id", "index"]
---

# VectorExtract

Extracts a single element from an input vector, converting VectorNumberData to NumberData.

## Configuration

- `id`: Unique identifier for the operator
- `index`: Zero-based index of the element to extract (must be non-negative)

```json
{
  "id": "ext1",
  "index": 1
}
```

## Ports

- Input Port 0: VectorNumberData
- Output Port 0: NumberData (scalar value at the specified index)

## Operation

For each input vector message, outputs the element at the specified index as a scalar NumberData message with the same timestamp.

| Time | Input (vector)    | Output (scalar) |
| ---- | ----------------- | --------------- |
| 1    | [10.0, 20.0, 30.0] | 20.0 (index=1) |
| 2    | [40.0, 50.0, 60.0] | 50.0 (index=1) |

## Error Handling

- Throws if `index` is negative at construction
- Throws at runtime if index is out of bounds for the input vector

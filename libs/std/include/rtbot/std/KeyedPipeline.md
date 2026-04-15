---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      K[{{key_index}}]
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
      examples: ["kp1"]
    key_index:
      type: integer
      description: Index of the key field in the input vector (classic mode)
      minimum: 0
      examples: [0]
    keyColumnIndices:
      type: array
      items:
        type: integer
      description: Column indices for computed key mode (polynomial hash computed internally)
    prototype:
      description: Prototype definition (object) or reference (string) for per-key sub-graph
      oneOf:
        - type: string
        - type: object
  required: ["id"]
---

# KeyedPipeline

Routes incoming VectorNumberData messages by a key field to per-key sub-pipeline instances. Each key gets an independent sub-graph instantiated from a Prototype definition with persistent state across messages.

## Configuration

### Classic mode

- `id`: Unique identifier for the operator
- `key_index`: Zero-based index of the key field in the input vector
- `prototype`: ID of a prototype defined in the program's `prototypes` section

```json
{
  "id": "kp1",
  "key_index": 0,
  "prototype": "stats_proto"
}
```

### Computed key mode

- `id`: Unique identifier for the operator
- `keyColumnIndices`: Array of column indices to include in the hash key
- `prototype`: ID of a prototype defined in the program's `prototypes` section

The operator internally computes a polynomial hash over the specified columns.

```json
{
  "id": "kp2",
  "keyColumnIndices": [0, 3],
  "prototype": "stats_proto"
}
```

## Ports

- Input Port 0: VectorNumberData (must contain the key at the specified index)
- Output Port 0: VectorNumberData (classic: key prepended; computed: prototype output directly)

## Operation

### Classic mode

For each incoming message:

1. Extract key value from input vector at `key_index`
2. Look up or create a sub-graph for this key
3. Deliver the full input vector to the sub-graph's entry operator
4. Collect output from the sub-graph, prepend key value at index 0
5. Emit on output port

### Computed key mode

For each incoming message:

1. Compute key as polynomial hash over `keyColumnIndices` columns
2. Look up or create a sub-graph for this key
3. Deliver the full input vector to the sub-graph's entry operator
4. Collect output from the sub-graph directly (no key prepend)
5. Emit on output port

Sub-graphs maintain persistent state across messages. A CumulativeSum inside a per-key sub-graph accumulates across all messages for that key.

### Example (classic mode)

```
Prototype: VectorExtract(index=1) -> CumulativeSum
Input:  t=1: [1.0, 100.0]  -> key=1, sum=100  -> output: [1.0, 100.0]
Input:  t=2: [2.0, 200.0]  -> key=2, sum=200  -> output: [2.0, 200.0]
Input:  t=3: [1.0, 150.0]  -> key=1, sum=250  -> output: [1.0, 250.0]
Input:  t=4: [2.0, 250.0]  -> key=2, sum=450  -> output: [2.0, 450.0]
```

## Features

- Dynamic sub-graph creation for new keys
- Optional new-key callback for runtime notification
- Full collect/restore serialization of all per-key states
- Handles both NumberData and VectorNumberData sub-graph outputs

## Error Handling

- Classic mode: Throws if `key_index` is negative at construction
- Classic mode: Throws at runtime if key_index is out of bounds for the input vector
- Computed key mode: Throws if `keyColumnIndices` is empty or contains negative indices

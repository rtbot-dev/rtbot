---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      KP
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
      examples: ["kp1"]
    prototype:
      description: Prototype definition (object) or reference (string) for per-key sub-graph
      oneOf:
        - type: string
        - type: object
  required: ["id"]
---

# KeyedPipeline

Routes incoming VectorNumberData messages by `msg->id` to per-key sub-pipeline instances. Each key gets an independent sub-graph instantiated from a Prototype definition with persistent state across messages.

## Configuration

- `id`: Unique identifier for the operator
- `prototype`: ID of a prototype defined in the program's `prototypes` section

```json
{
  "id": "kp1",
  "prototype": "stats_proto"
}
```

## Ports

- Input Port 0: VectorNumberData (key comes from `msg->id`)
- Output Port 0: VectorNumberData (sub-graph output, `msg->id` set to key)

## Operation

For each incoming message:

1. Read key from `msg->id`
2. Look up or create a sub-graph for this key
3. Deliver the full input vector to the sub-graph's entry operator
4. Collect output from the sub-graph, set `msg->id = key` on output
5. Emit on output port

Sub-graphs maintain persistent state across messages. A CumulativeSum inside a per-key sub-graph accumulates across all messages for that key.

### Example

```
Prototype: VectorExtract(index=0) -> CumulativeSum
Input:  t=1, id=1: [100.0]  -> key=1, sum=100  -> output id=1: [100.0]
Input:  t=2, id=2: [200.0]  -> key=2, sum=200  -> output id=2: [200.0]
Input:  t=3, id=1: [150.0]  -> key=1, sum=250  -> output id=1: [250.0]
Input:  t=4, id=2: [250.0]  -> key=2, sum=450  -> output id=2: [450.0]
```

## Features

- Dynamic sub-graph creation for new keys
- Optional new-key callback for runtime notification
- Full collect/restore serialization of all per-key states
- Handles both NumberData and VectorNumberData sub-graph outputs

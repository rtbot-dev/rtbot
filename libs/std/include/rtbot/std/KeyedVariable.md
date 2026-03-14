---
behavior:
  buffered: false
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      KV[{{mode}}]
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
      examples: ["kv1"]
    mode:
      type: string
      description: Mode of operation. Use exists to emit boolean membership result or lookup to emit numeric value.
    default_value:
      type: number
      default: 0.0
      description: Value emitted in lookup mode when the key is not found
  required: ["id"]
---

# KeyedVariable

HashMap-backed reference-data store. Maintains a mutable key→value map updated from a changelog stream and answers point-in-time lookup queries.

## Ports

| Port | Direction | Type | Description |
| --- | --- | --- | --- |
| i1 | data | VectorNumberData [key, value] | Updates map: `map[key] = value`; NaN value deletes the key |
| c1 | control | NumberData | Key to look up; emits result on o1 |
| c2 | control | NumberData | Heartbeat — advances the timeline without changing state |
| o1 | output | BooleanData or NumberData | Result of the lookup (type depends on `mode`) |

## Properties

| Property | Type | Default | Description |
| --- | --- | --- | --- |
| `id` | string | — | Operator identifier |
| `mode` | string | "exists" | Output type: "exists" (boolean) or "lookup" (numeric value) |
| `default_value` | number | 0.0 | Returned in lookup mode when the key is absent |

## Processing Order (same timestamp)

1. c2 advances `heartbeat_time`
2. i1 updates the HashMap
3. c1 queries are resolved against the updated state

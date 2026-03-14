---
behavior:
  buffered: true
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      MKC({{window_size}})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
      examples: ["mkc1"]
    window_size:
      type: integer
      description: Sliding window size (number of messages) for key counting
      minimum: 1
      examples: [20]
  required: ["id", "window_size"]
---

# MovingKeyCount

For each incoming key value, counts how many times that key appeared in the last `window_size` messages (sliding window).

- **Input port i1**: NumberData — key value
- **Output port o1**: NumberData — count of this key in the current window

## Properties

| Property | Type | Description |
| --- | --- | --- |
| `id` | string | Operator identifier |
| `window_size` | integer ≥ 1 | Sliding window size |

## Use Case

Pre-filter for `HAVING MOVING_COUNT(N) > threshold` before a `KeyedPipeline`, so only active (frequently-seen) keys pass through.

---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      \text{WinMinMax}({{window_size}},{{mode}})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
      examples: ["wmm1"]
    window_size:
      type: integer
      description: The size of the sliding window
      minimum: 1
      examples: [10]
    mode:
      type: string
      description: Whether to track minimum or maximum over the sliding window
      enum: ["min", "max"]
      default: "min"
  required: ["id", "window_size"]
---

# WindowMinMax

The WindowMinMax operator computes the rolling minimum or maximum over the last N values using an O(1) amortized monotonic deque. No output is emitted until the window is full.

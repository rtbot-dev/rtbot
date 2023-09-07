---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \sum
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
  required: ["id"]
---

# CumulativeSum

Outputs the cumulative sum of the message's values that pass through it.

$$y(t_n)=\sum_{i=1}^{n} x(t_i)$$
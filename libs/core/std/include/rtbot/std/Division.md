---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      /
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
  required: ["id"]
---

# Division

Synchronizes two streams and computes its division. Synchronization mechanism inherited from `Join`.
    $$y(t_n)=x_1(t_n) / x_2(t_n)$$

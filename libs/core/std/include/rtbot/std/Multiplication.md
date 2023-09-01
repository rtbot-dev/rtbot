---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      \times
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
  required: ["id"]
---

# Multiplication

Synchronizes two streams and emits the multiplication between the values for a given $t$.
Synchronization mechanism inherited from `Join`.

$$y(t_n)=x_1(t_n) \times x_2(t_n)$$
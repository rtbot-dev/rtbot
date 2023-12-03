---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      &&
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
  required: ["id"]
---

# And

Synchronizes two streams and emits the and-logic between the values for a given $t_n$. 
Synchronization mechanism inherited from `Join`.

$$y(t_n)=x_1(t_n) \wedge x_2(t_n)$$

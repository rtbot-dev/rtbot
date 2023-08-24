---
behavior:
  buffered: true
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \Delta
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
  required: ["id"]
---

# Difference
Computes the difference between the values of two consecutive messages. Emits 
in the last one time.

$$y(t_n)=x(t_n) - x(t_{n-1})$$
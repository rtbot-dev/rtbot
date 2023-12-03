---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      x^{{{value}}}
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    value:
      type: number
      description: The exponent.
  required: ["id", "value"]
---

# Power

Emits messages with values equal to the power specified:

$$y(t_n)=x(t_n)^{p}$$
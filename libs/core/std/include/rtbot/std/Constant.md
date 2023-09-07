---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \equiv {{value}}
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    value:
      type: number
      description: The constant to emit when required.
  required: ["id", "value"]
---

# Constant

A constant operator. Always emits the same value: $y(t_n)=C$.
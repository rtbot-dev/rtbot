---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \times {{value}}
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    value:
      type: number
      description: The factor to use to scale the messages.
  required: ["id", "value"]
---

# Scale

Emits messages with values multiplied by the number specified:
$$y(t_n)= C \times x(t_n)$$
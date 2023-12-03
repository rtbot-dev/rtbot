---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      + {{value}}
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    value:
      type: number
      description: The constant to add to the incoming messages.
  required: ["id", "value"]
---

# Add

TODO: review this
Adds a constant specified value to each input message.
$$y(t_n)= x(t_n) + C$$

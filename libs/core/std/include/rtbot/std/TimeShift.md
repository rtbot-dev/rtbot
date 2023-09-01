---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      {{value}}\Delta t
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    dt:
      type: integer
      default: 1
      minimum: 1
      description: The constant that defines the time grid.
    times:
      type: integer
      default: 1
      description: The multiplier to apppy.
  required: ["id"]
---

# TimeShift

Adds a constant $n \times \Delta t$ to each input message time.

$$y(t_n)= x(t_n) + n \times \Delta t$$
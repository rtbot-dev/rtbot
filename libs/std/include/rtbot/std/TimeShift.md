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
      examples:
        - -1
      description: The constant that defines the time grid.
    times:
      type: integer
      default: 1
      examples:
        - 2
      description: The multiplier to apppy.
  required: ["id"]
---

# TimeShift

Inputs: `i1`  
Outputs: `o1`

Adds a specified constant (dt * times) to each message time it receives on `i1` regardless the value field of the message.

The `Add` operator does not hold a message buffer on `i1`, so it emits a modified version of the message through `o1` right after it receives a message on `i1`.

$$y(t_n)= x(t_n) + n \times \Delta t$$
---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      Var({{default}})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    value:
      type: number
      description: The default value of the variable
      default: 0
  required: ["id"]
---

# Variable

A variable is a special operator designed to store stateful computations.
It has one data input port and one control port. Messages received through
the data input port are considered as definitions for the values of the variable
from the time of the message up to the next different message time.

Messages received through the control port will trigger the emission of the value
of the variable, according with the time present in the control message, through
the output port.
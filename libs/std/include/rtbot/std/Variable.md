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
      examples:
        - 1.618
      description: The default value of the variable
      default: 0
  required: ["id"]
---

# Variable

Inputs: `i1`  
Outputs: `o1`  
Controls: `c1`

The `Variable` operator is a special operator designed to store stateful computations.

The `Variable` has one data input port `i1`, one control port `c1` and one output port `o1`. 
Messages received through the data input port are considered definitions for the values 
of the variable from the time of the message up to the next different message time.

Messages received through the control port `c1` will trigger the emission of the value
of the variable, according with the time present in the control message, through `o1`.
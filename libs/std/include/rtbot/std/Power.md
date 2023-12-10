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
      examples:
        - 2.71
      description: The exponent.
  required: ["id", "value"]
---

# Power

Inputs: `i1`  
Outputs: `o1`

Emits messages with values equal to the power of the specified exponent represented by the number (value) regardless the time field of the message.

The `Power` operator does not hold a message buffer on `i1`, so it emits a modified version of the message through `o1` right after it receives a message on `i1`.

$$y(t_n)=x(t_n)^{p}$$
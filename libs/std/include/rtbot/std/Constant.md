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
      examples:
        - 3.14
        - 2.718
      description: The constant to emit when required.
  required: ["id", "value"]
---

# Constant

Inputs: `i1`  
Outputs: `o1`

Emits the same value regardless the time field of the message received on `i1`. 

The `Constant` operator does not hold a message buffer on `i1`, so it emits a message with a constant value field through `o1` right after it receives a message on `i1`.

$$y(t_n)=C$$.

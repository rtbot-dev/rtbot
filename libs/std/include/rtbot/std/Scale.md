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
      examples:
        - 2.71
      description: The factor to use to scale the messages.
  required: ["id", "value"]
---

# Scale

Inputs: `i1`  
Outputs: `o1`

Scales the message value using the provided number (value) as coefficient regardless the time field of the message. 

The `Scale` operator does not hold a message buffer on `i1`, so it emits a modified version of the message through `o1` right after it receives a message on `i1`.

$$y(t_n)= C \times x(t_n)$$
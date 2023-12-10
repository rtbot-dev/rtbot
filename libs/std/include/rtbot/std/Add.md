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
      examples:
        - 1.0
      description: The constant to add to the incoming messages.
  required: ["id", "value"]
---

# Add

Inputs: `i1`  
Outputs: `o1`

Adds a specified constant (value) to each message value it receives on `i1` regardless the time field of the message. 

The `Add` operator does not hold a message buffer on `i1`, so it emits a modified version of the message through `o1` right after it receives a message on `i1`.

$$y(t_n)= x(t_n) + C$$

---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \mathbb{1}
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
  required: ["id"]
---

# Identity

Inputs: `i1`  
Outputs: `o1`

The `Identity` operator forwards all the messages it receives without modifying them. 

The `Identity` operator does not hold a message buffer on `i1`, so it emits the message through `o1` right after it receives a message on `i1`.

$$y(t_n)= x(t_n)$$
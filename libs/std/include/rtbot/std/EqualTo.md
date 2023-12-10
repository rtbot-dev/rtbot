---
behavior:
  buffered: false
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      if = {{value}}
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
      description: The reference value
  required: ["id", "value"]
---

# EqualTo

Inputs: `i1`  
Outputs: `o1`

Emits only the messages received with a value equal to the provided number  otherwise it emits an empty message {}. 

The `EqualTo` operator does not hold a message buffer on `i1`, so it emits the message through `o1` right after it receives a message on `i1` but only if the received message value is equal to the provided number otherwise it emits an empty message {}.
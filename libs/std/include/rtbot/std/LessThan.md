---
behavior:
  buffered: false
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      < {{value}}
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    value:
      type: integer
      description: The reference value
  required: ["id", "value"]
---

# LessThan

Inputs: `i1`  
Outputs: `o1`

Emits only the messages received with a value less than the provided number (value) otherwise it emits an empty message {}. 

The `LessThan` operator does not hold a message buffer on `i1`, so it emits a the message through `o1` right after it receives a message on `i1` but only if the received message value is less than the provided number (value) otherwise it emits an empty message {}.
---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      Count
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
  required: ["id"]
---

# Count

Inputs: `i1`  
Outputs: `o1`

Counts how many messages have passed through it. 

The `Count` operator does not hold a message buffer on `i1`, so it emits a message containing the amount of messages it has received on `i1` through `o1` right after it receives a message on `i1`.
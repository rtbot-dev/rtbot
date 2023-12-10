---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \sum
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
  required: ["id"]
---

# CumulativeSum

Inputs: `i1`  
Outputs: `o1`

Emits the cumulative sum of the messages values that have passed through it. 

The `CumulativeSum` operator does not hold a message buffer on `i1`, so it emits a message containing the the cumulative sum of the messages values it has received on `i1` through `o1` right after it received a message on `i1`.

$$y(t_n)=\sum_{i=1}^{n} x(t_i)$$
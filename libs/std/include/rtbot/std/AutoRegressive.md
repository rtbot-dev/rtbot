---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      AR({{#each coeff}}{{this}}{{#unless @last}},{{/unless}}{{/each}})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    coeff:
      type: array
      description: The list of auto-regression coefficients.
      examples: [[1.1, 0.9]]
      minItems: 1
      items:
        type: number
  required: ["id", "coeff"]
---

# AutoRegressive

Inputs: `i1`  
Outputs: `o1`

Implements a classical auto-regressive model. 

The `AutoRegressive` operator does not hold a message buffer on `i1`, so it emits a modified version of the message through `o1` right after it receives a message on `i1`.

$$y(t_n)=c_{1} y(t_{n-1}) + ... + c_N y(t_{t-N})$$

---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      Linear({{#each coeff}}{{this}}{{#unless @last}},{{/unless}}{{/each}})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    coeff:
      type: array
      description: The list of coefficients.
      minItems: 2
      items:
        type: number
  required: ["id"]
---

# Linear


Synchronizes input streams and emits a linear combination of the values for a given $t$.
Synchronization mechanism inherited from `Join`.

$$y(t_n)=c_1 x_1(t_n) + c_2 x_2(t_n) + ... + c_N x_N(t_n)$$
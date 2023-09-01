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
      examples: [[1, 2]]
      minItems: 1
      items:
        type: number
  required: ["id", "coeff"]
---

# AutoRegressive

Implements classical auto-regressive output.
    $$y(t_n)=c_{1} y(t_{n-1}) + ... + c_N y(t_{t-N})$$
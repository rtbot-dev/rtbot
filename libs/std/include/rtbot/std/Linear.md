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
      examples: 
        - [1, 1, -1]
      description: The list of coefficients.
      minItems: 2
      items:
        type: number
  required: ["id"]
---

# Linear

Inputs: `i1`...`iN` where N is defined by the length of `coeff`.  
Outputs: `o1`

Synchronizes input streams and emits a linear combination of the values for a given $t_n$.

The synchronization mechanism is inherited from the `Join` operator. The `Linear` operator holds a message buffer on `i1`...`iN`
respectively, it emits a modified version of the synchronized messages from `i1`...`iN` as the linear combination
of its values and the coefficients in `coeff` through `o1` right after the synchronization takes place, if no synchronization 
occurs then an empty message {} is emitted through `o1`.

$$y(t_n)=c_1 x_1(t_n) + c_2 x_2(t_n) + ... + c_N x_N(t_n)$$
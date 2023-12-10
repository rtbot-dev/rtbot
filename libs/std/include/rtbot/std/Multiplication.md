---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      \times
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
  required: ["id"]
---

# Multiplication

Inputs: `i1` `i2`  
Outputs: `o1`

Synchronizes two streams (given by `i1` and `i2`) and emits the multiplication of the values for a given $t_n$.

The synchronization mechanism is inherited from the `Join` operator. The `Multiplication` operator holds a message buffer on `i1` and `i2` respectively, it emits a modified version of the synchronized messages from `i1` and `i2` as the multiplication of its values through `o1` right after the synchronization takes place, if no synchronization occurs then an empty message {} is emitted through `o1`.

$$y(t_n)=x_1(t_n) \times x_2(t_n)$$
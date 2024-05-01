---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      ||
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
  required: ["id"]
---

# LessThanStream

Inputs: `i1` `i2`  
Outputs: `o1`

Synchronizes two streams (given by `i1` and `i2`) and emits the message in `i1` if its value is less than the value of `i2` for the synchronized $t_n$.

The synchronization mechanism is inherited from the `Join` operator. The `GreaterThanStream` operator holds a message buffer on `i1` and `i2` respectively, after synchronization occurs it emits the message in `i1` through `o1` if it happens to be less than the message value in `i2` and it discard the message in `i2`, if no synchronization occurs then an empty message {} is emitted through `o1`.

$$y(t_n) = x_1(t_n) if x_1(t_n) < x_2(t_n)$$

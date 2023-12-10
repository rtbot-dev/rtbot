---
behavior:
  buffered: true
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \sigma_{{value}}
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    n:
      type: number
      examples:
        - 20
      description: The number of grid steps to use to compute the standard deviation.
  required: ["id", "n"]
---

# StandardDeviation

Inputs: `i1`  
Outputs: `o1`

Computes the standard deviation (ED) within the time window specified by the provided integer (n).

The `StandardDeviation` operator holds a message buffer on `i1` with a size defined by the length of the provided integer (n). Once the message buffer on `i1` gets filled it calculates the ED and emits a message through `o1` right after the message buffer on `i1` gets filled. The value field of the emitted message is the calculated ED and the time field is the time of the newest message on the buffer.

$$y(t_n) = \frac{\sum_{i=1}^{N} (x(t_i) - \bar{x})^2}{N-1}$$

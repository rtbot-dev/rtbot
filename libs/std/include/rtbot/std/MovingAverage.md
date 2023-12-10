---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      MA({{ n }})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    n:
      type: integer
      examples:
        - 20
      description: The window size, in grid steps, to be used in the computation.
  required: ["id", "n"]
---

# MovingAverage

Inputs: `i1`  
Outputs: `o1`

Computes the moving average (MA) within the time window specified by the provided integer (n).

The `MovingAverage` operator holds a message buffer on `i1` with a size defined by the length of the provided integer (n). Once the message buffer on `i1` gets filled it calculates the MA and emits a message through `o1` right after the message buffer on `i1` gets filled. The value field of the emitted message is the calculated MA and the time field is the time of the newest message on the buffer.

$$y(t_n)= \frac{1}{N}(x(t_n) + x(t_{n-1}) + ... + x(t_{n-N-1}))$$
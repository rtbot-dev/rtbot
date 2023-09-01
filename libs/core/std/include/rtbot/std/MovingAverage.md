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
      description: The window size, in grid steps, to be used in the computation.
  required: ["id", "n"]
---

# MovingAverage

Computes the moving average within the time window specified in grid steps.

$$y(t_n)= \frac{1}{N}(x(t_n) + x(t_{n-1}) + ... + x(t_{n-N-1}))$$
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
      description: The number of grid steps to use to compute the standard deviation.
  required: ["id", "n"]
---

# StandardDeviation

Computes the standard deviation of the sample within the time window specified in
grid steps.

$$y(t_n) = \frac{\sum_{i=1}^{N} (x(t_i) - \bar{x})^2}{N-1}$$

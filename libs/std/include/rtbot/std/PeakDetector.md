---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      \hat{Ex}_{[t_n, t_n+{{n}}\Delta t]}
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

# PeakDetector

Finds a local extreme within the time window specified.
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
      examples:
        - 3.0
      description: The window size, in grid steps, to be used in the computation.
  required: ["id", "n"]
---

# PeakDetector

Inputs: `i1`  
Outputs: `o1`

Finds a local maximum (PEAK) within the time window specified by the provided integer (n).

The `PeakDetector` operator holds a message buffer on `i1` with a size defined by the length of
the provided integer (n). Once the message buffer on `i1` gets filled it calculates the PEAK
and emits a message through `o1` right after the message buffer on `i1` gets filled. The value
field of the emitted message is the calculated PEAK and the time field is the time of the
newest message on the buffer.

The implementation uses a robust local extreme definition: if the message in the middle of
the time window has a value such that it is larger than any other message value then it is
considered that it is a good local extreme. Typical job is to find the optimal time window
size according to the application.

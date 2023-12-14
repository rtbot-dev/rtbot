---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      Hermite({{ dt }})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    dt:
      type: integer
      examples:
        - 5
      description: The size to transform the input grid steps, if any, on dt steps
  required: ["id", "dt"]
---

# HermiteResampler

Inputs: `i1`  
Outputs: `o1`

The `HermiteResampler` operator resamples the input stream using the provided value (dt). The implemented algorithm can be found on http://paulbourke.net/miscellaneous/interpolation/ and it has been labeled as Hermite Interpolation.

The `HermiteResampler` operator holds a message buffer on `i1` with a size of 4. Once the message buffer on `i1` gets filled it resamples the interval and emits a vector of messages through `o1` right after the message buffer on `i1` gets filled. The amount of messages returned in the resulting vector depend on how many dt intervals fall into the analized buffer.
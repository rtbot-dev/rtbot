---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      fft
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    N:
      type: integer
      default: 7
      minimum: 2
      description: Use to compute the fft matrix size, which will be $2^N$
      examples: [3, 4, 5]
    skip:
      type: integer
      default: 127
      minimum: 0
      description: Controls how many messages to skip before the next computation of the fft. This is useful for applications were the fft is not needed to be computed for each received message. Notice that a value different than 0 changes the throughput of the signal.
      examples: [127, 255, 100]
    emitPower:
      type: boolean
      description: Indicates whether the power correspondent to the different frequencies should be computed and emitted through the output ports `p1`, ..., `pM`, where $M=2^N$
      default: true
      examples: [true, false]
    emitRePart:
      type: boolean
      description: Indicates whether the real part of the amplitude correspondent to the different frequencies should be computed and emitted through the output ports `re1`, ..., `reM`, where $M=2^N$
      default: true
      examples: [true, false]
    emitImPart:
      type: boolean
      description: Indicates whether the imaginary part of the amplitude correspondent to the different frequencies should be computed and emitted through the output ports `im1`, ..., `imM`, where $M=2^N$
      default: true
      examples: [true, false]
  required: ["id"]
---

# FastFourierTransform

Inputs: `i1`  
Outputs:

- frequencies `w1`...`wM`
- power `p1`...`pM` if `emitPower` is true
- real part `re1`...`reM` if `emitRePart` is true
- imaginary part `im1`...`imM` if `emitImPart` is true

where `M` is equal to $2^n$.

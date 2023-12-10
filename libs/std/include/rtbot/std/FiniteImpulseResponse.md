---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      FIR({{ b_{0}, b_{1}, b{2} ... b_{N-1} }})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    coeff:
      type: array
      examples: 
        - [1, 0, -1]
      description: The vector of coefficients to be combined with the buffered message values.
      items:
        type: number
  required: ["id", "coeff"]
---

# FiniteImpulseResponse

Inputs: `i1`  
Outputs: `o1`

Computes the finite impulse response (FIR) within the time window specified by the length of the provided vector (coeff). 

The `FiniteImpulseResponse` operator holds a message buffer on `i1` with a size defined by the length of the provided vector (coeff). Once the message buffer on `i1` gets filled it calculates the FIR and emits a message through `o1` right after the message buffer on `i1` gets filled. The value field of the emitted message is the calculated FIR and the time field is the time of the newest message on the buffer.

$$y(t_n)=  b_{0} \cdot x(t_n) + b_{1} \cdot x(t_{n-1}) + ... + b_{N-1} \cdot x(t_{n-N-1})$$

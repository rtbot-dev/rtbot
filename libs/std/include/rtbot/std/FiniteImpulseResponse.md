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

Computes the finite impulse response (FIR) within the time window specified by the length of the vector as follows:

$$y(t_n)=  b_{0} \cdot x(t_n) + b_{1} \cdot x(t_{n-1}) + ... + b_{N-1} \cdot x(t_{n-N-1})$$

---
behavior:
  buffered: true
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      FIR({{#each coeff}}{{this}}{{#unless @last}},{{/unless}}{{/each}})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    coeff:
      type: array
      description: The vector of FIR coefficients
      examples:
        - [1.0, -0.5, 0.25]
      minItems: 1
      items:
        type: number
    tolerance:
      type: number
      description: Threshold below which coefficients are considered zero
      default: 1e-10
      examples: [1e-6]
  required: ["id", "coeff"]
---

# FiniteImpulseResponse

The Finite Impulse Response (FIR) operator implements a classical FIR filter, computing the weighted sum of a sliding window of input values.

## Buffer Size

The buffer size equals the length of the coefficient vector. For N coefficients, the operator maintains an N-sample buffer.

## Output Computation

For input sequence x[n] and coefficients b[k], the output y[n] is computed as:

$$y[n] = \sum_{k=0}^{N-1} b[k] \cdot x[n-k]$$

## Example Operation

Time series showing input values and corresponding outputs for coefficients [0.5, 0.3, 0.2]:

| Time | Input | Output | Notes                       |
| ---- | ----- | ------ | --------------------------- |
| 1    | 1.0   | -      | Buffering                   |
| 2    | 2.0   | -      | Buffering                   |
| 3    | 3.0   | 1.6    | 3.0×0.5 + 2.0×0.3 + 1.0×0.2 |
| 5    | 4.0   | 2.4    | 4.0×0.5 + 3.0×0.3 + 2.0×0.2 |
| 6    | 5.0   | 3.2    | 5.0×0.5 + 4.0×0.3 + 3.0×0.2 |

Note how output starts after buffer fills and continues with constant throughput.

## Error Handling

Throws std::runtime_error if:

- Coefficient vector is empty
- Input message has invalid type

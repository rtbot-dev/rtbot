---
behavior:
  buffered: true
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      IIR({{b}},{{a}})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    b:
      type: array
      description: Feed-forward (input) coefficients vector
      minItems: 1
      items:
        type: number
      examples: [[1.0, -0.5, 0.25]]
    a:
      type: array
      description: Feedback (output) coefficients vector
      minItems: 1
      items:
        type: number
      examples: [[0.3, 0.1]]
  required: ["id", "b", "a"]
---

# InfiniteImpulseResponse

The InfiniteImpulseResponse operator implements a general-purpose IIR (Infinite Impulse Response) digital filter. It combines feed-forward terms from the input signal with feedback terms from previous outputs.

## Port Configuration

- Input Port 0: Accepts NumberData messages containing the input signal values
- Output Port 0: Emits NumberData messages containing the filtered output values

## Operation

The filter implements the difference equation:

y(n) = {b₀x[n] + b₁x[n-1] + ... + bₘ₋₁x[n-M+1]} - {a₁y[n-1] + a₂y[n-2] + ... + aₙy[n-N]}

Where:

- x[n]: Input values
- y[n]: Output values
- b: Feed-forward coefficients (M coefficients)
- a: Feedback coefficients (N coefficients)
- M: Length of b vector
- N: Length of a vector

## Behavior

1. Input Handling:
   - Maintains a buffer of M most recent input values
   - Requires M input samples before producing first output
2. Output Generation:
   - Uses available feedback terms (gradually incorporating up to N terms)
   - Maintains a buffer of N most recent output values
   - Uses same timestamp as corresponding input message

## Example Operation

Consider a first-order IIR filter with b=[1.0], a=[0.5]:

| Time | Input | Output | Calculation               |
| ---- | ----- | ------ | ------------------------- |
| 1    | 1.0   | 1.0    | y = 1.0 (no feedback yet) |
| 2    | 1.0   | 0.5    | y = 1.0 - 0.5×1.0         |
| 3    | 1.0   | 0.75   | y = 1.0 - 0.5×0.5         |
| 4    | 1.0   | 0.625  | y = 1.0 - 0.5×0.75        |

## Implementation Notes

1. Buffer Management:

   - Input buffer (x\_) limited to size M
   - Output buffer (y\_) limited to size N
   - FIFO behavior: oldest values removed first

2. Error Handling:

   - Requires at least one coefficient in both b and a vectors
   - Validates message types

3. State Persistence:
   - Serializes both input and output buffers
   - Maintains filter state across save/restore operations

## Common Applications

- Low-pass filtering
- High-pass filtering
- Band-pass filtering
- Signal smoothing
- Audio processing
- Control systems

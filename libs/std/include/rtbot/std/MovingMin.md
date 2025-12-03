# MovingMin Operator

The `MovingMin` operator computes the minimum of the most recent `window_size` values. It emits a value once the
internal buffer is full and thereafter slides forward one sample at a time.

## Parameters
- `window_size` (integer, required): Number of samples inside the moving window.

## Ports
- Input `NumberData`
- Output `NumberData`

## Behaviour
- Produces no output until `window_size` samples have been observed.
- After the buffer is full, outputs the minimum of the current window for each new sample.

# MovingVariance Operator

`MovingVariance` emits the sample variance of the most recent `window_size` numeric samples.

## Parameters
- `window_size` (integer, required): Number of samples in the sliding window.

## Ports
- Input `NumberData`
- Output `NumberData`

## Behaviour
- Produces no output until the buffer holds `window_size` samples.
- Afterwards emits the variance (using unbiased n-1 denominator) for each new sample.

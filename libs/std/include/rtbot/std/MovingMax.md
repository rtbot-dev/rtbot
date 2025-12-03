# MovingMax Operator

The `MovingMax` operator computes the maximum of the latest `window_size` samples. It behaves similarly to a pandas
rolling maximum with `min_periods=window_size`.

## Parameters
- `window_size` (integer, required): Number of samples in the sliding window.

## Ports
- Input `NumberData`
- Output `NumberData`

## Behaviour
- Emits no data until the buffer contains `window_size` samples.
- Afterwards emits the maximum of the current window for each new input sample.

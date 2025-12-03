# PctChange Operator

`PctChange` emits the percentage change between consecutive numeric samples. The first sample is ignored because there
is no prior value to compare against.

## Ports
- Input `NumberData`
- Output `NumberData`

## Behaviour
- For each incoming value `x_t`, when a previous sample `x_{t-1}` exists and `x_{t-1} != 0`, emits `(x_t - x_{t-1}) / x_{t-1}` with timestamp `t`.
- No output is produced for the first message or when the previous value is zero.

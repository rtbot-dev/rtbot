# CumulativeProduct Operator

`CumulativeProduct` maintains the running product of all received numeric samples.

## Ports
- Input `NumberData`
- Output `NumberData`

## Behaviour
- Multiplies the incoming value into the internal state and emits the updated product for each message.
- The running product resets to `1.0` when the operator is reset.

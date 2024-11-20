---
behavior:
  buffered: true
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \Delta t = {{interval}}
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    interval:
      type: integer
      examples:
        - 5
      description: The time interval between emissions.
      minimum: 1
  required: ["id", "interval"]
---

# ConstantResampler

Inputs: `i1`  
Outputs: `o1`

The `ConstantResampler` operator transforms a variable throughput input signal into a constant throughput output signal by emitting values at fixed time intervals. The operator maintains causal consistency while handling grid-aligned messages with special precision.

## Behavior

### Initialization

When receiving its first message at time t₀, the operator:

- Stores the initial value
- Sets up first emission point at t₀ + dt
- Does not emit

### Regular Operation

For subsequent messages, the operator follows these rules:

1. **Grid-Aligned Messages**:

   - If a message arrives exactly at a grid point (t = nextEmit)
   - Emits immediately with the current message's value
   - Updates nextEmit to t + dt

2. **Between Grid Points**:

   - If message time is between grid points
   - Stores the value for future emissions
   - No immediate emission

3. **Past Grid Points**:

   - If message time is past one or more grid points
   - Emits at each missed grid point using the last known value
   - Maintains causal consistency by using values known at each emission time

4. **Large Gaps**:
   - For large time gaps between messages
   - Emits at all intermediate grid points
   - Uses the last known value before each emission point

### Key Characteristics

- Lazy evaluation: only emits when receiving messages
- Maintains constant time intervals between emissions
- Preserves causal consistency
- Special handling for grid-aligned messages
- Buffers only one message (the most recent value)
- State is maintained between calls and can be serialized/restored

## Examples

### Basic Grid Alignment

```
Input: dt = 5
t=1: Receive(1, 10.0)  -> No emission, nextEmit = 6
t=3: Receive(3, 30.0)  -> No emission
t=6: Receive(6, 60.0)  -> Emit(6, 60.0), nextEmit = 11
t=8: Receive(8, 80.0)  -> No emission
```

### Large Gap with Multiple Emissions

```
Input: dt = 3
t=1: Receive(1, 10.0)  -> No emission, nextEmit = 4
t=9: Receive(9, 90.0)  -> Emit sequence:
                          - (4, 10.0)  [using last known value]
                          - (7, 10.0)  [using last known value]
                          nextEmit = 10
```

### Mixed Grid-Aligned and Non-Aligned

```
Input: dt = 5
t=1:  Receive(1, 10.0)  -> No emission, nextEmit = 6
t=6:  Receive(6, 60.0)  -> Emit(6, 60.0)  [grid-aligned]
t=13: Receive(13, 130.0) -> Emit(11, 60.0) [using previous value]
```

## Error Handling

- Throws if dt ≤ 0
- Throws if input port count is incorrect

## State Management

The operator maintains:

- Current interval (dt)
- Next emission time
- Last received value
- Initialization status

All state can be serialized and restored for system persistence.

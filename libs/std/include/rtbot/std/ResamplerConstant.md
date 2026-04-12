---
behavior:
  buffered: true
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \Delta t={{interval}}
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    interval:
      type: integer
      description: Fixed time interval between emissions
      minimum: 1
      examples: [5]
    t0:
      type: integer
      description: Optional starting time for the resampling grid
      examples: [10]
    snapFirst:
      type: integer
      description: When 1, snaps to the current grid point on first message and uses each incoming value immediately (default 0)
      enum: [0, 1]
      default: 0
  required: ["id", "interval"]
---

# ResamplerConstant

Transforms an irregularly sampled input stream into a regularly sampled output stream by emitting values at fixed time intervals.

Input port (0): Data stream to be resampled  
Output port (0): Resampled data stream

## Operation

### Default mode (snapFirst=0)

1. **Initialization**: The first message stores the held value but does not produce output.
2. **Grid Points**: Values are emitted at times k×interval + t0.
   - If t0 specified: first grid point is the next grid point after the first message.
   - If t0 not specified: first grid point is first_message_time + interval.
3. **Value Selection**: At each grid point, the last received value is emitted. If a message arrives exactly on a grid point, its value is used.

### Snap-first mode (snapFirst=1)

Requires t0. On initialization the resampler snaps to the current grid point (at or before the first message) instead of skipping to the next one. Each incoming value is applied immediately before the emission loop, so the message's value is used at the current grid point rather than being deferred.

This eliminates one grid-interval of latency compared to default mode. It is designed for pipelines where upstream aggregates arrive just past the grid boundary (e.g. a bin average flushed at the start of the next bin) and should be emitted at the grid point they represent, not the next one.

## Examples

### Default mode, no t0 (interval=5)

| Time | Input | Output | Notes                   |
| ---- | ----- | ------ | ----------------------- |
| 1    | 10.0  | -      | First message sets t0=1 |
| 3    | 20.0  | -      | Store value             |
| 6    | -     | 20.0   | Grid point: t0+5        |
| 9    | 30.0  | -      | Store value             |
| 11   | -     | 30.0   | Grid point: t0+10       |

### Default mode, t0=10 (interval=5)

| Time | Input | Output | Notes                   |
| ---- | ----- | ------ | ----------------------- |
| 3    | 10.0  | -      | Before first grid point |
| 8    | 20.0  | -      | Still before t0         |
| 10   | -     | 20.0   | First grid point        |
| 15   | -     | 20.0   | Next grid point         |
| 16   | 30.0  | -      | Store for next point    |
| 20   | -     | 30.0   | Next grid point         |

### Snap-first mode, t0=0 (interval=10)

| Time | Input | Output | Notes                                  |
| ---- | ----- | ------ | -------------------------------------- |
| 8    | 10.0  | 10.0   | Snaps to grid point 0, emits at t=0    |
| 15   | 20.0  | 20.0   | Value applied first, emits at t=10     |
| 25   | 30.0  | 30.0   | Value applied first, emits at t=20     |

Compare with default mode: the first message at t=8 would produce no output and the first emission would be at t=10 with the held value 10.0. With snap-first, the output starts one interval earlier.

## Error Handling

- Throws if interval ≤ 0
- Throws on message type mismatch

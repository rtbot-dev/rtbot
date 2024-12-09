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
  required: ["id", "interval"]
---

# ResamplerConstant

Transforms an irregularly sampled input stream into a regularly sampled output stream by emitting values at fixed time intervals.

Input port (0): Data stream to be resampled  
Output port (0): Resampled data stream

## Operation

1. **Grid Points**: Values are emitted at times k×interval + t0

   - If t0 specified: Uses provided t0
   - If t0 not specified: Uses first message time as t0

2. **Value Selection**:
   - Uses last known value before grid point
   - Uses actual value if message arrives exactly on grid point

## Examples

Given interval=5 (no t0):

| Time | Input | Output | Notes                   |
| ---- | ----- | ------ | ----------------------- |
| 1    | 10.0  | -      | First message sets t0=1 |
| 3    | 20.0  | -      | Store value             |
| 6    | -     | 20.0   | Grid point: t0+5        |
| 9    | 30.0  | -      | Store value             |
| 11   | -     | 30.0   | Grid point: t0+10       |

Given interval=5, t0=10:

| Time | Input | Output | Notes                   |
| ---- | ----- | ------ | ----------------------- |
| 3    | 10.0  | -      | Before first grid point |
| 8    | 20.0  | -      | Still before t0         |
| 10   | -     | 20.0   | First grid point        |
| 15   | -     | 20.0   | Next grid point         |
| 16   | 30.0  | -      | Store for next point    |
| 20   | -     | 30.0   | Next grid point         |

## Error Handling

- Throws if interval ≤ 0
- Throws on message type mismatch

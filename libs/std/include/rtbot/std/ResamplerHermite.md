---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      R_H(\Delta t{{#if t0}}, t_0{{/if}})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
      examples: ["resampler1"]
    interval:
      type: integer
      description: The time interval between resampled points
      minimum: 1
      examples: [5]
    t0:
      type: integer
      description: Optional start time for first emission
      examples: [100]
  required: ["id", "interval"]
---

# ResamplerHermite

The ResamplerHermite operator performs smooth interpolation between irregularly spaced input points to generate regularly spaced output points. It uses cubic Hermite spline interpolation to maintain continuity of both values and derivatives.

## Configuration

### Required Parameters

- `id`: Unique identifier for the operator
- `interval`: Time interval between resampled points (must be > 0)
- `t0` (optional): Starting time for first emission

### Example Configuration

```json
{
  "id": "resampler1",
  "interval": 5,
  "t0": 100
}
```

## Ports

Inputs: `1`
Outputs: `1`

## Operation

1. Maintains a 4-point sliding window buffer for interpolation
2. Initializes emission times based on t0 or first complete buffer
3. Emits interpolated values at regular intervals within valid ranges
4. Preserves function characteristics like local extrema

## Implementation Details

The operator uses a Buffer base class with:

- Fixed 4-point window size for Hermite interpolation
- Feature flags disabled for statistical tracking
- Efficient interpolation computation

### Interpolation Algorithm

The cubic Hermite spline interpolation:

1. Uses 4 points to ensure C1 continuity
2. Computes tangents from adjacent points
3. Applies tension parameter (default: 0) for shape control
4. Interpolates using Hermite basis functions

### Time Handling

- Input times must be strictly increasing
- Output times are aligned to interval boundaries
- Optional t0 parameter sets first emission time
- Interpolation only within valid buffer range

### Error Handling

The operator will throw exceptions for:

- Invalid interval (≤ 0)
- Non-monotonic input times
- Invalid message types
- Buffer overflow conditions

## Use Cases

Ideal for:

- Signal resampling
- Regular sampling of irregular data
- Smooth interpolation
- Time series alignment
- Data rate conversion

## Performance Characteristics

- O(1) memory usage (fixed buffer size)
- O(1) per message processing
- Output rate determined by interval
- Interpolation preserves C1 continuity

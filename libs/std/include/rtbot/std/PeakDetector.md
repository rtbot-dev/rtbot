---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      Peak({{n}})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    window_size:
      type: integer
      description: The size of the sliding window for peak detection (must be odd and >= 3)
      minimum: 3
      examples: [3, 5, 7]
  required: ["id", "window_size"]
---

# PeakDetector

Inputs: Port 0  
Outputs: Port 0

The PeakDetector operator identifies local maxima in a time series by examining values within a sliding window. A peak is detected when the center value in the window is strictly greater than all other values in the window.

The operator maintains a fixed-size buffer of messages. When the buffer is full, it checks if the center point represents a local maximum. If a peak is detected, the center point is emitted through the output port.

## Configuration

- Window size must be odd (to have a clear center point)
- Window size must be at least 3
- Larger windows provide more context for peak detection

## Example

Consider a window size of 3 and the following input sequence:

```
Time  Value  Output
1     1.0    -
2     2.0    -
3     1.0    2.0 (emitted at t=3)
4     0.5    -
5     1.5    -
6     0.8    1.5 (emitted at t=6)
```

## State Management

The operator maintains:

- Fixed-size buffer of input messages
- Window size configuration

All state can be serialized and restored for system persistence.

## Error Handling

Throws exceptions for:

- Even window sizes
- Window sizes less than 3
- Invalid message types

## Performance Considerations

- O(1) memory usage (fixed buffer size)
- O(window_size) computation per message
- Peak detection only occurs when buffer is full

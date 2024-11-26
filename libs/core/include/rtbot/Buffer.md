---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      Buffer({{window_size}})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    window_size:
      type: integer
      description: The size of the sliding window buffer
      minimum: 1
      examples: [20]
  required: ["id", "window_size"]
---

# Buffer

The Buffer operator is an abstract base class that provides efficient buffering functionality with optional statistical tracking features. It maintains a sliding window of fixed size and provides facilities for derived operators to process the buffered data.

## Features

### Compile-Time Optional Features

The Buffer operator supports the following optional features that can be enabled at compile time:

1. **Sum Tracking**

   - Maintains a running sum of all values in the buffer
   - Efficiently updated when values enter/exit the buffer
   - O(1) access time

2. **Mean Tracking**

   - Tracks the running mean (average) of buffered values
   - Uses numerically stable online algorithm
   - O(1) access time

3. **Variance Tracking**
   - Computes running variance and standard deviation
   - Uses Welford's online algorithm for numerical stability
   - Supports standard deviation calculation
   - O(1) access time

### Core Functionality

- Fixed-size sliding window buffer
- FIFO (First-In-First-Out) behavior
- Automatic management of buffer size
- State serialization and restoration
- Type-safe message handling

## Configuration

Features can be enabled/disabled using the `BufferFeatures` struct:

```cpp
struct BufferFeatures {
    static constexpr bool TRACK_SUM = true;      // Enable sum tracking
    static constexpr bool TRACK_MEAN = true;     // Enable mean tracking
    static constexpr bool TRACK_VARIANCE = true; // Enable variance tracking
};
```

## Template Parameters

1. `T`: The data type to be buffered

   - Must satisfy RtBot's data type requirements
   - Must support serialization/deserialization
   - Common types: NumberData, BooleanData, VectorNumberData

2. `Features`: Feature configuration (optional)
   - Defaults to BufferFeatures
   - Can be customized to enable/disable features
   - Zero overhead for disabled features

## Statistical Methods

When corresponding features are enabled:

- `sum()`: Returns the sum of all values in the buffer
- `mean()`: Returns the arithmetic mean of buffered values
- `variance()`: Returns the sample variance
- `standard_deviation()`: Returns the sample standard deviation

## Buffer Interface

- `buffer_size()`: Current number of elements in buffer
- `buffer_full()`: Whether buffer has reached capacity
- `buffer()`: Direct access to underlying deque (const)

## Implementation Notes

1. **Memory Efficiency**

   - O(N) memory usage where N is window size
   - No temporary allocations during normal operation
   - Efficient reuse of memory

2. **Numerical Stability**

   - Uses single-pass algorithms for statistics
   - Minimizes numerical errors in running calculations
   - Handles large numbers of updates gracefully

3. **Performance**
   - O(1) updates for all operations
   - Efficient handling of data entry/exit
   - No recomputation of statistics

## State Management

The Buffer operator maintains:

- Current window of values
- Statistical accumulators (if enabled)
- Message order and timing

State can be serialized and restored, preserving:

- Buffer contents
- Statistical state
- Configuration parameters

## Error Handling

The operator will throw exceptions for:

- Invalid window size (must be > 0)
- Type mismatches on port input
- Buffer overflow conditions

## Usage Example

Creating a moving average operator using Buffer:

```cpp
class MovingAverage : public Buffer<NumberData> {
public:
    MovingAverage(std::string id, size_t window)
        : Buffer<NumberData>(id, window) {}

protected:
    bool process_message(const Message<NumberData>* msg) override {
        // Only emit when buffer is full
        return buffer_full();
    }
};
```

## Performance Considerations

1. **Memory Usage**

   - Linear with window size
   - Constant overhead per enabled feature
   - No dynamic allocations during processing

2. **Computational Complexity**

   - Message insertion: O(1)
   - Statistical updates: O(1)
   - Memory moves: O(1) amortized

3. **Numerical Considerations**
   - Stable accumulation of sums
   - Accurate variance computation
   - Minimal floating-point error accumulation

## Best Practices

1. **Feature Selection**

   - Enable only needed features
   - Use appropriate window sizes
   - Consider memory constraints

2. **Type Safety**

   - Use appropriate data types
   - Handle type conversion explicitly
   - Validate input data

3. **Error Handling**
   - Check buffer status
   - Validate window sizes
   - Handle edge cases

## Use Cases

The Buffer operator is particularly useful for:

- Moving averages and other sliding window statistics
- Signal smoothing and filtering
- Pattern detection over time windows
- Real-time statistical analysis

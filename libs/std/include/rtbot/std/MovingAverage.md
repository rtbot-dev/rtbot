---
behavior:
  buffered: true
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      MA({{window_size}})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
      examples: ["ma1"]
    window_size:
      type: integer
      description: The size of the moving average window
      minimum: 1
      examples: [20]
  required: ["id", "window_size"]
---

# MovingAverage

The MovingAverage operator calculates a rolling average over a sliding window of numeric values. Each time a new value arrives, it's added to the buffer and included in the average calculation. When the buffer reaches its specified size, the oldest value is removed before adding new ones.

## Configuration

### Required Parameters

- `id`: Unique identifier for the operator
- `window_size`: Number of values to include in the moving average window (must be >= 1)

### Example Configuration

```json
{
  "id": "ma1",
  "window_size": 20
}
```

## Port Configuration

### Inputs

- `i1`: Single input port accepting NumberData messages

### Outputs

- `o1`: Single output port emitting NumberData messages with averaged values

## Operation

1. Incoming messages are added to a sliding window buffer
2. The average is calculated as the sum of values divided by the current buffer size
3. Output messages contain:
   - Same timestamp as input message
   - Value field contains the current average
4. Initial output begins as soon as the first value is received
5. The buffer automatically manages the window size by removing oldest values when full

## Implementation Details

The operator:

- Uses a simple sum/count average calculation
- Maintains a running sum for efficiency
- Automatically handles buffer overflow
- Processes messages in order of arrival
- Preserves input message timestamps

### Memory Usage

- O(N) where N is the window_size
- Fixed memory footprint once buffer fills
- No dynamic allocation during normal operation

### Performance Characteristics

- Message insertion: O(1)
- Average calculation: O(1)
- Memory cleanup: O(1)
- Output generation: O(1)

## Error Handling

The operator will throw exceptions for:

- Invalid window size (must be >= 1)
- Invalid message types
- Type mismatches on input port

## Use Cases

Ideal for:

- Signal smoothing
- Noise reduction
- Trend detection
- Data preprocessing
- Real-time monitoring

## Examples

### Basic Usage

```cpp
// Create operator with window size 5
auto ma = std::make_unique<MovingAverage>("ma1", 5);

// Process some values
ma->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
ma->receive_data(create_message<NumberData>(2, NumberData{20.0}), 0);
ma->execute();

// Access current average
double avg = ma->get_average();
```

### Pipeline Integration

```cpp
auto input = make_number_input("in1");
auto ma = std::make_unique<MovingAverage>("ma1", 10);
auto output = make_number_output("out1");

input->connect(ma.get());
ma->connect(output.get());
```

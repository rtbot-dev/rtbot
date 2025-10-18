---
behavior:
  buffered: true
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      MS({{window_size}})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
      examples: ["ms1"]
    window_size:
      type: integer
      description: The size of the moving sum window
      minimum: 1
      examples: [20]
  required: ["id", "window_size"]
---

# MovingSum

The MovingSum operator calculates a rolling sum over a sliding window of numeric values. Each time a new value arrives, it's added to the buffer and included in the sum calculation. When the buffer reaches its specified size, the oldest value is removed before adding new ones.

## Configuration

### Required Parameters

- `id`: Unique identifier for the operator
- `window_size`: Number of values to include in the moving sum window (must be >= 1)

### Example Configuration

```json
{
  "id": "ms1",
  "window_size": 20
}
```

## Port Configuration

### Inputs

- `i1`: Single input port accepting NumberData messages

### Outputs

- `o1`: Single output port emitting NumberData messages with added values

## Operation

1. Incoming messages are added to a sliding window buffer
2. The sum is calculated as the sum of all values when the buffer gets full
3. Output messages contain:
   - Same timestamp as input message
   - Value field contains the current sum
4. Initial output begins as soon as the window size length of the buffer is reached
5. The buffer automatically manages the window size by removing oldest values when full

## Implementation Details

The operator:

- Uses a simple sum calculation
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

- Area below a curve
- Data preprocessing
- Real-time monitoring

## Examples

### Basic Usage

```cpp
// Create operator with window size 5
auto ms = std::make_unique<MovingSum>("ms1", 2);

// Process some values
ms->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
ms->receive_data(create_message<NumberData>(2, NumberData{20.0}), 0);
ms->execute();

// Access current average
double sum = ms->sum();
```

### Pipeline Integration

```cpp
auto input = make_number_input("in1");
auto ms = std::make_unique<MovingSum>("ms1", 10);
auto output = make_number_output("out1");

input->connect(ms.get());
ms->connect(output.get());
```

---
behavior:
  buffered: true
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \Delta
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
      examples: ["diff1"]
    use_oldest_time:
      type: boolean
      description: Whether to use the newer message's time (true) or older message's time (false) for output
      default: true
      examples: [true]
  required: ["id"]
---

# Difference

The Difference operator calculates sequential differences between pairs of numeric values in a data stream. It maintains a buffer of size 2 to compute differences between consecutive values.

## Configuration

### Required Parameters

- `id`: Unique identifier for the operator
- `use_oldest_time`: (Optional) Controls timestamp of output messages
  - `true`: Use newer message's timestamp
  - `false`: Use older message's timestamp

### Example Configuration

```json
{
  "id": "diff1",
  "use_oldest_time": true
}
```

## Port Configuration

### Inputs

- Port 0: Accepts NumberData messages

### Outputs

- Port 0: Emits NumberData messages with computed differences

## Operation

The operator emits messages with values calculated as:

```
output_value = newest_value - oldest_value
```

### Message Flow Example

| Time | Input Value | Output Value | Notes                          |
| ---- | ----------- | ------------ | ------------------------------ |
| 1    | 10.0        | -            | First value buffered           |
| 2    | 15.0        | 5.0          | First difference (15.0 - 10.0) |
| 4    | 12.0        | -3.0         | Next difference (12.0 - 15.0)  |
| 5    | 20.0        | 8.0          | Next difference (20.0 - 12.0)  |

### Key Characteristics

- Buffer size: 2 messages
- Output timing: Configurable via use_oldest_time
- Processing: O(1) per message
- Memory usage: O(1) fixed
- Must receive 2 messages before first output

## Error Handling

The operator will throw exceptions for:

- Invalid message types on input port
- Type mismatches on input

## Use Cases

Ideal for:

- Rate of change calculations
- Delta detection
- Trend analysis
- Signal differentiation

## Example Usage

```cpp
// Create operator using newer message timestamps
auto diff = std::make_unique<Difference>("diff1", true);

// Process some values
diff->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
diff->receive_data(create_message<NumberData>(2, NumberData{15.0}), 0);
diff->execute();

// Access difference
const auto& output = diff->get_output_queue(0);
// Output will contain a message with value 5.0 at time 2
```

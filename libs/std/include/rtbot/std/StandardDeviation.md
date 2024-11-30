---
behavior:
  buffered: true
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \sigma_{{{window_size}}}
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
      examples: ["sd1"]
    window_size:
      type: integer
      description: The size of the window for standard deviation calculation
      minimum: 1
      examples: [20]
  required: ["id", "window_size"]
---

# StandardDeviation

The StandardDeviation operator calculates the standard deviation over a sliding window of numeric values. It emits the standard deviation value whenever a new value arrives and the buffer is full.

## Configuration

### Required Parameters

- `id`: Unique identifier for the operator
- `window_size`: Number of values to include in the standard deviation calculation (must be >= 1)

### Example Configuration

```json
{
  "id": "sd1",
  "window_size": 20
}
```

## Port Configuration

### Inputs

- Port 0: Single input port accepting NumberData messages

### Outputs

- Port 0: Single output port emitting NumberData messages with standard deviation values

## Operation

The operator:

1. Maintains a sliding window of the most recent `window_size` values
2. Calculates standard deviation using the formula: σ = √(Σ(x - μ)²/(n-1))
3. Emits output messages containing:
   - Same timestamp as the latest input message
   - Value field contains the calculated standard deviation
4. Output begins only when the buffer is full

### Example Message Flow

| Time | Input Value | Output Value | Notes          |
| ---- | ----------- | ------------ | -------------- |
| 1    | 2.0         | -            | Buffer filling |
| 3    | 4.0         | -            | Buffer filling |
| 5    | 6.0         | -            | Buffer filling |
| 7    | 8.0         | 2.582        | First output   |
| 10   | 10.0        | 2.582        | Sliding window |

## Statistical Details

The standard deviation is calculated using a numerically stable algorithm that:

- Maintains running sums and means
- Uses online variance calculation
- Properly handles buffer overflow
- Provides accurate results even with large numbers

## Error Handling

The operator will throw exceptions for:

- Invalid window size (must be >= 1)
- Invalid message types
- Type mismatches on input port

## Use Cases

Ideal for:

- Volatility measurement
- Signal variability analysis
- Anomaly detection
- Data quality monitoring
- Process control

## Examples

### Basic Usage

```cpp
// Create operator with window size 5
auto sd = std::make_unique<StandardDeviation>("sd1", 5);

// Process some values
sd->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
sd->receive_data(create_message<NumberData>(2, NumberData{12.0}), 0);
sd->execute();
```

### Pipeline Integration

```cpp
auto input = make_number_input("in1");
auto sd = std::make_unique<StandardDeviation>("sd1", 10);
auto output = make_number_output("out1");

input->connect(sd.get());
sd->connect(output.get());
```

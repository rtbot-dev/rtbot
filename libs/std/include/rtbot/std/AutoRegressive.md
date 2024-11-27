---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      AR({{#each coefficients}}{{this}}{{#unless @last}},{{/unless}}{{/each}})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    coefficients:
      type: array
      description: The auto-regression coefficients to apply to previous values
      minItems: 1
      items:
        type: number
      examples: [[0.5, 0.3, -0.1]]
  required: ["id", "coefficients"]
---

# AutoRegressive

The AutoRegressive operator implements a classical auto-regressive model, calculating each output as a weighted sum of previous inputs. It uses a sliding window buffer to maintain the required history of values.

## Ports

- Input Port 0: Receives numeric values to process
- Output Port 0: Emits auto-regressive calculations

## Operation

For each input value x(t), once the buffer is full, the operator calculates:

$$y(t) = c_0x(t) + c_1x(t-1) + ... + c_{n-1}x(t-n-1)$$

where:

- cᵢ are the coefficients provided at construction
- n is the number of coefficients
- x(t-i) represents the value from i time steps ago

The operator maintains a buffer of size n (number of coefficients) and produces output once the buffer is full.

### Example Sequence

Consider an AR(2) model with coefficients [0.5, 0.3]:

| Time | Input | Buffer State | Output | Notes                   |
| ---- | ----- | ------------ | ------ | ----------------------- |
| 1    | 2.0   | [2.0]        | -      | Buffer filling          |
| 2    | 3.0   | [2.0, 3.0]   | 1.6    | `0.5 * 2.0 + 0.3 * 2.0` |
| 3    | 4.0   | [3.0, 4.0]   | 2.1    | `0.5 * 3.0 + 0.3 * 2.0` |
| 4    | 5.0   | [4.0, 5.0]   | 2.9    | `0.5 * 4.0 + 0.3 * 3.0` |

## Implementation Details

- Uses Buffer base class for efficient sliding window management
- Maintains coefficients in order of most recent to oldest application
- O(n) computation where n is number of coefficients
- Output timestamp matches input timestamp
- Output begins as soon as buffer is full

## Error Handling

Throws std::runtime_error if:

- Empty coefficient vector provided
- Invalid port indices used
- Type mismatches on input

## Use Cases

Ideal for:

- Time series prediction
- Signal processing
- Financial modeling
- System identification
- Control systems

## Example Usage

```cpp
// Create AR(2) model with coefficients [0.5, 0.3]
std::vector<double> coeffs{0.5, 0.3};
auto ar = std::make_unique<AutoRegressive>("ar1", coeffs);

// Process values
ar->receive_data(create_message<NumberData>(1, NumberData{2.0}), 0);
ar->receive_data(create_message<NumberData>(2, NumberData{3.0}), 0);
ar->execute();
```

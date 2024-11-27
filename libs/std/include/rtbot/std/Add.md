---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      + {{value}}
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
      examples: ["add1"]
    value:
      type: number
      description: The constant value to add to incoming messages
      examples: [5.0]
  required: ["id", "value"]
---

# Add

The Add operator performs constant addition on a stream of numeric values. Each incoming value has a constant added to it while preserving the message timestamp.

## Configuration

### Required Parameters

- `id`: Unique identifier for the operator
- `value`: Constant value to add to each message

### Example Configuration

```json
{
  "id": "add1",
  "value": 5.0
}
```

## Port Configuration

### Inputs

- `i1`: Single input port accepting NumberData messages

### Outputs

- `o1`: Single output port emitting NumberData messages with added values

## Operation

1. Each incoming message has the configured value added to it
2. Output messages contain:
   - Same timestamp as input message
   - Value field contains input value plus constant
3. Messages are processed immediately (no buffering)
4. Order of messages is preserved

## Mathematical Description

For an input message x(tₙ) at time tₙ, the operator produces:

y(tₙ) = x(tₙ) + C

where C is the configured constant value.

## Implementation Details

The operator:

- Processes one message at a time
- Maintains no internal state besides the constant
- Performs no buffering
- Preserves message order
- Supports state serialization

### Memory Usage

- O(1) memory usage
- No message buffering
- Fixed state size

### Performance Characteristics

- Message processing: O(1)
- No memory allocation during normal operation
- Immediate throughput

## Error Handling

The operator will throw exceptions for:

- Invalid message types
- Type mismatches on input port
- State restoration errors

## Use Cases

Ideal for:

- Signal offsetting
- Baseline adjustment
- Data normalization
- Constant bias addition
- Numeric calibration

## Examples

### Basic Usage

```cpp
// Create operator that adds 5 to each value
auto add = make_add("add1", 5.0);

// Process a message
add->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
add->execute();
```

### Pipeline Integration

```cpp
auto input = make_number_input("in1");
auto add = make_add("add1", 5.0);
auto output = make_number_output("out1");

input->connect(add.get());
add->connect(output.get());
```

## Best Practices

1. **Numerical Considerations**

   - Consider potential overflow when adding large values
   - Be aware of floating-point precision limits
   - Use appropriate value types for your data range

2. **Performance**

   - No need to batch messages as processing is immediate
   - Can handle high-frequency message streams
   - No performance degradation over time

3. **Pipeline Design**
   - Often used early in pipelines for calibration
   - Can be chained with other arithmetic operators
   - Consider combining multiple adds into a single operation

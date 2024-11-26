---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      Out
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    port_types:
      type: array
      description: An array of port types that define the input and output ports
      minItems: 1
      items:
        type: string
        enum: ["number", "boolean", "vector_number", "vector_boolean"]
  required: ["id", "port_types"]
---

# Output

The Output operator is used to push data out of a program. It immediately forwards incoming messages to corresponding output ports while preserving data types. Each input port (i1...iN) has a corresponding output port (o1...oN) of the same type.

## Port Configuration

Ports are configured using type specifiers:

- `number`: For scalar numeric values
- `boolean`: For boolean values
- `vector_number`: For numeric vectors
- `vector_boolean`: For boolean vectors

The number and types of ports are specified at construction and cannot be modified afterward.

### Example Configuration

```yaml
# Single numeric output
operators:
  - id: "out1"
    type: Output
    port_types: ["number"]

# Multi-port mixed type output
operators:
  - id: "out2"
    type: Output
    port_types:
      - "number"          # Temperature
      - "boolean"         # Status flag
      - "vector_number"   # Acceleration vector
```

## Operation

The Output operator:

1. Receives messages on input ports (i1...iN)
2. Immediately forwards each message to its corresponding output port
3. Preserves message order and timing
4. Maintains type safety across port pairs

### Message Flow

```
Input Port    Output Port    Behavior
i1 (number) → o1 (number)   Direct forwarding
i2 (vector) → o2 (vector)   No buffering
```

No buffering or synchronization is performed - messages are forwarded as soon as they are received.

## Factory Functions

Convenience creators for common configurations:

```cpp
// Single port outputs
auto out1 = make_number_output("out1");
auto out2 = make_boolean_output("out2");
auto out3 = make_vector_number_output("out3");

// Custom multi-port output
auto out4 = std::make_unique<Output>("out4",
    std::vector<std::string>{
        PortType::NUMBER,
        PortType::BOOLEAN
    });
```

## Error Handling

The operator will throw exceptions for:

- Invalid port type specifications
- Port type mismatches on input
- Empty port type configuration

## State Management

The Output operator:

- Maintains no internal state
- Requires no message buffering
- Has constant memory usage
- Provides consistent throughput

## Performance Considerations

- O(1) processing per message
- No memory growth over time
- Immediate message forwarding
- Zero-copy message passing where possible

## Use Cases

Ideal for:

- Program termination points
- Data export interfaces
- Stream monitoring points
- Pipeline output collection
- Multi-stream data recording

## Best Practices

1. **Type Safety**

   - Use factory functions for common configurations
   - Verify port types match connected operators
   - Handle all output ports in receiving systems

2. **Performance**

   - Keep output processing light
   - Handle outputs asynchronously when possible
   - Monitor output rates for bottlenecks

3. **Design**
   - Group related outputs together
   - Use meaningful port ordering
   - Document port type expectations

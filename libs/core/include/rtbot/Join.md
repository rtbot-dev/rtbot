---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      \cap
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    port_types:
      type: array
      description: An array of port types that define the input and output ports
      minItems: 2
      items:
        type: string
        enum: ["number", "boolean", "vector_number", "vector_boolean"]
  required: ["id", "port_types"]
---

# Join

The Join operator synchronizes multiple data streams by matching messages with identical timestamps. Each input port has a corresponding output port of the same type, allowing for synchronization of heterogeneous data types.

## Port Configuration

The Join operator requires at least two ports. Each port can be configured with one of the following types:

- `number`: For scalar numeric values
- `boolean`: For boolean values
- `vector_number`: For numeric vector values
- `vector_boolean`: For boolean vector values

Each input port (i1, i2, ..., iN) has a corresponding output port (o1, o2, ..., oN) of the same type.

## Operation

The Join operator:

1. Maintains a message buffer for each input port
2. Tracks timestamps of messages in each buffer
3. Identifies common timestamps across all input ports
4. Forwards synchronized messages to corresponding output ports
5. Cleans up processed messages and messages that can no longer be synchronized

### Synchronization Process

For each execution cycle:

1. Find the oldest timestamp that exists in all input buffers
2. If a common timestamp is found:
   - Forward each message with that timestamp to its corresponding output port
   - Remove the processed messages and any older messages from the buffers
3. If no common timestamp exists:
   - Store messages for future synchronization
   - Clean up any messages that can no longer be synchronized

### Example

```cpp
// Create a Join with three different port types
auto join = std::make_unique<Join>("join1",
    std::vector<std::string>{
        PortType::NUMBER,
        PortType::BOOLEAN,
        PortType::VECTOR_NUMBER
    });

// Input messages with same timestamp (t=1)
join->receive_data(create_message<NumberData>(1, NumberData{42.0}), 0);
join->receive_data(create_message<BooleanData>(1, BooleanData{true}), 1);
join->receive_data(create_message<VectorNumberData>(1, VectorNumberData{{1.0, 2.0}}), 2);

// Execute synchronization
join->execute();

// Messages will be forwarded with preserved types
```

### Message Flow Example

```
Time  Port1   Port2   Output
1     A       -       Buffer
2     B       -       Buffer
2     -       X       Emit A,X (t=2)
3     C       Y       Emit C,Y (t=3)
```

## State Management

The Join operator maintains:

- Input message buffers for each port
- Timestamp tracking for synchronization
- Port type configuration

State can be serialized and restored, preserving:

- Port configuration
- Timestamp tracking information
- Message buffer states

## Error Handling

The operator will throw exceptions for:

- Invalid port type specifications
- Insufficient number of ports (minimum 2)
- Type mismatches on port input
- State restoration with mismatched configuration

## Factory Functions

Convenience functions are provided for common configurations:

```cpp
// Create a binary join for numbers
auto join = make_number_join("join1", 2);

// Create a three-way join for booleans
auto join = make_boolean_join("join1", 3);
```

## Performance Considerations

- Message buffers grow with timestamp misalignment
- Old messages are automatically cleaned up after they can no longer be synchronized
- Processing time scales with the number of ports and buffer sizes

## Use Cases

The Join operator is particularly useful for:

- Synchronizing sensor data from multiple sources
- Combining heterogeneous data streams
- Event correlation across different systems
- Time-aligned data processing pipelines

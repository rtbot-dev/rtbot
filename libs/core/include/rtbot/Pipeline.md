---
behavior:
  buffered: true
  throughput: variable
view:
  shape: rectangle
  latex:
    template: |
      Pipeline({{input_types}}, {{output_types}})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    input_port_types:
      type: array
      description: List of port types for input ports
      minItems: 1
      items:
        type: string
        enum: ["number", "boolean", "vector_number", "vector_boolean"]
    output_port_types:
      type: array
      description: List of port types for output ports
      minItems: 1
      items:
        type: string
        enum: ["number", "boolean", "vector_number", "vector_boolean"]
    operators:
      type: array
      description: List of operators in the pipeline
      items:
        type: object
        properties:
          id:
            type: string
            description: Operator identifier
          type:
            type: string
            description: Operator type
        required: ["id", "type"]
    connections:
      type: array
      description: List of connections between operators
      items:
        type: object
        properties:
          from:
            type: string
            description: Source operator id
          to:
            type: string
            description: Destination operator id
          fromPort:
            type: string
            description: Source operator port
          toPort:
            type: string
            description: Destination operator port
        required: ["from", "to"]
    entryOperator:
      type: string
      description: Id of the operator that receives pipeline input
    outputMappings:
      type: object
      description: Mappings from internal operator outputs to pipeline outputs
      additionalProperties:
        type: object
        description: Port mappings for an operator
        additionalProperties:
          type: string
          description: Pipeline output port that this operator port maps to
  required:
    ["id", "input_port_types", "output_port_types", "operators", "connections", "entryOperator", "outputMappings"]
---

# Pipeline

The Pipeline operator encapsulates a mesh of interconnected operators, acting as a composite operator that can be used like any other operator in the system. It maintains internal state isolation by resetting its internal operator mesh whenever output is produced.

## Operator Configuration

### Input/Output Ports

Both input and output ports are configured through port type arrays:

```json
{
  "id": "my_pipeline",
  "input_port_types": ["number", "boolean"],
  "output_port_types": ["number", "vector_number"]
}
```

### Internal Configuration

The pipeline's internal operator mesh is configured through the following methods:

- `register_operator(id, operator)`: Adds an operator to the pipeline
- `set_entry(operator_id, port)`: Designates the entry point operator and port
- `add_output_mapping(operator_id, operator_port, pipeline_port)`: Maps internal operator outputs to pipeline outputs
- `connect(from_id, to_id, from_port, to_port)`: Creates connections between internal operators

## Operation

1. **Message Reception**

   - Messages received on input ports are forwarded to the designated entry operator
   - Message types are preserved during forwarding

2. **Internal Processing**

   - Messages flow through the internal operator mesh
   - Each internal operator processes messages according to its own rules
   - Connections determine the message flow path

3. **Output Collection**

   - Output mappings define which internal operator outputs map to pipeline outputs
   - Messages are copied from mapped internal outputs to pipeline outputs

4. **State Reset**
   - Whenever the `Pipeline` produces any output, the pipeline resets all internal operators
   - Reset occurs before the next processing cycle
   - Ensures state isolation between processing cycles

## Example

```cpp
// Create a pipeline with number input and output
auto pipeline = std::make_shared<Pipeline>(
    "my_pipeline",
    std::vector<std::string>{PortType::NUMBER},
    std::vector<std::string>{PortType::NUMBER}
);

// Configure internal operators
auto ma = std::make_shared<MovingAverage>("ma1", 10);
auto peak = std::make_shared<PeakDetector>("peak1", 5);

pipeline->register_operator(ma);
pipeline->register_operator(peak);

// Set entry point
pipeline->set_entry("ma1");

// Connect operators
pipeline->connect("ma1", "peak1");

// Map peak detector output to pipeline output
pipeline->add_output_mapping("peak1", 0, 0);
```

## Error Handling

The operator throws exceptions for:

- Invalid port type specifications
- Missing entry point configuration
- Invalid operator references in connections
- Invalid port indices in mappings
- Type mismatches in connections

## State Management

The Pipeline operator manages:

1. Internal operator registry
2. mesh topology
3. Entry point configuration
4. Output mappings
5. Reset state tracking

All internal state is automatically reset when:

- The pipeline produces output
- Before the next processing cycle begins

## Performance Considerations

- Message copying occurs at input and output boundaries
- Internal message passing uses standard operator mechanisms
- State reset may impact performance when frequent outputs occur
- Consider grouping operations to minimize reset frequency

## Use Cases

Ideal for:

- Encapsulating complex processing logic
- Creating reusable operator meshs
- Isolating processing state
- Building hierarchical processing systems

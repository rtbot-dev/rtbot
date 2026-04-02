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

The Pipeline operator implements **segment-scoped computation**. It encapsulates a mesh of interconnected operators and uses an external control signal to define segments. Data accumulates within a segment (while the control key is stable), and output is emitted only when the key changes — at the segment boundary.

## Control Port

The Pipeline has a single control port that receives `NumberData` messages. The numeric value carried by each control message is interpreted as a **segment key**. The key can be any numeric value, enabling expressions like:

- `FLOOR(ts / 86400)` for day-scoped segments
- `SUM(DIFF(is_on) > 0)` for cycle-scoped segments
- A constant `1.0` for single-segment accumulation (output only on key change)

Control messages must be paired with data messages at the same timestamp. The Pipeline synchronizes data and control by timestamp, discarding unpaired messages from whichever side is ahead.

## Operation

### 1. Timestamp Pairing

Each processing step requires both a data message and a control message with matching timestamps. If timestamps don't match, the Pipeline discards the older message (data or control) and retries until a pair is found.

### 2. Key Tracking

The control message value is the current segment key. The Pipeline tracks the current key across processing steps:

- **First message**: establishes the initial key.
- **Same key**: data is forwarded to internal operators, and their output (if any) is buffered.
- **Different key (key change)**: a segment boundary is detected.

### 3. Segment Boundary (Key Change)

When the key changes:

1. **Emit**: the most recently buffered output is placed on the Pipeline's output ports, stamped with the boundary timestamp (the timestamp of the key-change message).
2. **Reset**: all internal operators are reset to their initial state.
3. **Continue**: the current data message is forwarded to the freshly reset internal operators, starting a new segment with the new key.

### 4. Internal Forwarding and Buffering

Within a segment (stable key), each data message is forwarded to the entry operator. The internal mesh executes, and if any mapped output operator produces output, it is stored in the output buffer (overwriting any previous buffer for that port). The buffer is **not** emitted to the Pipeline's output ports until a key change occurs.

### 5. Output

The Pipeline only produces output at segment boundaries (key changes). The emitted message carries the boundary timestamp, not the timestamp of the internal computation that produced the buffered value.

## JSON Configuration

```json
{
  "type": "Pipeline",
  "id": "my_pipeline",
  "input_port_types": ["number"],
  "output_port_types": ["number"],
  "operators": [
    {"type": "MovingAverage", "id": "ma1", "window_size": 3}
  ],
  "connections": [],
  "entryOperator": "ma1",
  "outputMappings": {
    "ma1": {"o1": "o1"}
  }
}
```

### Connecting the Control Port

In the program-level connections array, use port name `c1` to route a signal to the Pipeline's control port (control ports use the `c` prefix):

```json
{
  "connections": [
    {"from": "input1", "to": "pipeline1", "fromPort": "o1", "toPort": "i1"},
    {"from": "input1", "to": "pipeline1", "fromPort": "o2", "toPort": "c1"},
    {"from": "pipeline1", "to": "output1", "fromPort": "o1", "toPort": "i1"}
  ]
}
```

## Example: Day-Scoped Moving Average

```json
{
  "operators": [
    {"type": "Input", "id": "input1", "portTypes": ["number", "number"]},
    {
      "type": "Pipeline",
      "id": "daily_ma",
      "input_port_types": ["number"],
      "output_port_types": ["number"],
      "operators": [
        {"type": "MovingAverage", "id": "ma", "window_size": 10}
      ],
      "connections": [],
      "entryOperator": "ma",
      "outputMappings": {
        "ma": {"o1": "o1"}
      }
    },
    {"type": "Output", "id": "output1", "portTypes": ["number"]}
  ],
  "connections": [
    {"from": "input1", "to": "daily_ma", "fromPort": "o1", "toPort": "i1"},
    {"from": "input1", "to": "daily_ma", "fromPort": "o2", "toPort": "c1"},
    {"from": "daily_ma", "to": "output1", "fromPort": "o1", "toPort": "i1"}
  ],
  "entryOperator": "input1",
  "output": { "output1": ["o1"] }
}
```

Input port `o1` carries data values. Input port `o2` carries a day index (e.g., `FLOOR(ts / 86400)`). When the day index changes, the Pipeline emits the last buffered MA output and resets the MA for the new day.

## Serialization

The Pipeline serializes:

- Its own segment state (current key, whether a key has been established)
- The output buffer (port → message mapping)
- All internal operators' state (recursively)

After deserialization, the Pipeline resumes from the exact same segment state, including any buffered but not-yet-emitted output.

## Error Handling

The operator throws exceptions for:

- Invalid port type specifications
- Missing entry point configuration (`entryOperator` not found)
- Invalid operator references in connections
- Invalid port indices in output mappings
- Entry operator having fewer data ports than the Pipeline

## Performance Considerations

- Internal output is buffered, not copied on every step — only the latest output per port is retained.
- Emission happens only at segment boundaries, reducing output frequency.
- State reset on key change may impact performance if segments are very short.
- Message synchronization (timestamp pairing) discards unmatched messages, so data and control must arrive in lockstep.

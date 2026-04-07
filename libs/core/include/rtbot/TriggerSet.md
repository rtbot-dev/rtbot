---
behavior:
  buffered: true
  throughput: variable
view:
  shape: rectangle
  latex:
    template: |
      TriggerSet({{input_type}}, {{output_type}})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    input_port_type:
      type: string
      description: Port type for the single input port (forwarded to entry operator data port 0)
      enum: ["number", "boolean", "vector_number", "vector_boolean"]
    output_port_type:
      type: string
      description: Port type for the single output port
      enum: ["number", "boolean", "vector_number", "vector_boolean"]
    operators:
      type: array
      description: List of operators in the trigger set
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
    entryOperator:
      type: string
      description: Id of the single operator that receives all trigger set inputs
    outputOperator:
      type: object
      description: The single operator/port whose output is forwarded as the trigger set output
      properties:
        id:
          type: string
          description: Output operator id
        port:
          type: string
          description: Output operator port name such as o1
      required: ["id"]
    connections:
      type: array
      description: List of connections between internal operators
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
  required:
    ["id", "input_port_type", "output_port_type", "operators", "entryOperator", "outputOperator"]
---

# TriggerSet

The `TriggerSet` operator encapsulates a mesh of interconnected operators behind a **single input port**, a **single entry operator**, and a **single output operator/port**. It behaves like an atomic trigger: each input message is fed into the internal mesh, and *if* the designated output operator emits a message on the designated port, that message is forwarded as the trigger set's only output and the entire internal mesh is reset.

This makes a `TriggerSet` a natural building block for "fire-once-then-rearm" detectors composed from smaller operators.

## Semantics

- **One input port → one entry operator.** The trigger set exposes a single input port whose type is fixed at construction time. Every message arriving there is forwarded to data port `0` of the single entry operator designated via `set_entry`.
- **One output operator and one output port.** A single internal `(operator, port)` pair, designated via `set_output`, is the only source of output. The trigger set itself exposes exactly one output port whose type is fixed at construction time.
- **One trigger per cycle.** Per input message: the entry operator runs, the output operator/port is checked, and if it produced anything, the *first* message on that queue is forwarded and the whole internal mesh is reset. The trigger set never emits more than one message per input message.
- **State isolation.** Because reset happens immediately after a trigger fires, no internal state survives across trigger events.

## Operator Configuration

### Input/Output Ports

Both input and output are a single port type, set at construction time:

```json
{
  "id": "my_trigger_set",
  "input_port_type": "number",
  "output_port_type": "number"
}
```

### Internal Configuration

The internal mesh is built through the following methods:

- `register_operator(op)`: Adds an operator to the trigger set
- `set_entry(operator_id)`: Designates the (single) entry operator
- `set_output(operator_id, op_port = 0)`: Designates the (single) output operator and port
- `connect(from_id, to_id, from_port, to_port)`: Creates a connection between internal operators

## Operation

1. **Message reception** — A message arriving on the trigger set's input port is forwarded to data port `0` of the entry operator.
2. **Internal processing** — The entry operator is executed; messages flow through the internal mesh according to the registered connections.
3. **Trigger check** — After processing each input message, the output operator's designated port is checked. If non-empty, its front message is cloned into the trigger set's output port.
4. **Reset** — As soon as a trigger fires, every internal operator is reset. The next input message starts from a clean internal state.

## Example

```cpp
// A trigger set with a single number input and a single number output
auto ts = std::make_shared<TriggerSet>(
    "my_trigger_set",
    PortType::NUMBER,
    PortType::NUMBER
);

// Internal operators
auto ma   = std::make_shared<MovingAverage>("ma1", 10);
auto peak = std::make_shared<PeakDetector>("peak1", 5);

ts->register_operator(ma);
ts->register_operator(peak);

// Single entry, single output
ts->set_entry("ma1");
ts->connect("ma1", "peak1");
ts->set_output("peak1", 0);
```

## Error Handling

The operator throws exceptions for:

- Invalid port type specifications
- Missing entry operator (`set_entry` not called before processing)
- Missing output operator (`set_output` not called before processing)
- Invalid operator references in connections or in `set_output`
- Output operator port index out of range
- Entry operator with no data ports

## State Management

The `TriggerSet` operator manages:

1. The internal operator registry
2. The internal mesh topology
3. The single entry operator
4. The single `(output operator, output port)` pair
5. Reset on every fired trigger

## Performance Considerations

- Message copying happens at the input boundary (into the entry operator) and at the output boundary (out of the output operator).
- A reset is performed after every fired trigger, so very high trigger rates incur proportional reset cost.
- Because at most one message is emitted per input message, downstream operators see a strictly rate-limited stream.

## Use Cases

Ideal for:

- Composite trigger/detector logic built from smaller operators
- "Fire once, then re-arm" patterns where internal state must not leak across events
- Encapsulating a detector mesh behind a clean single-output interface

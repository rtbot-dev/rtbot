---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \circ
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    portTypes:
      type: array
      examples:
        - ["number", "boolean"]
      description: List of port types to create. Valid types are 'number', 'boolean', 'vector_number' and 'vector_boolean'
      items:
        type: string
        enum: ["number", "boolean", "vector_number", "vector_boolean"]
      minItems: 1
  required: ["id", "portTypes"]
---

# Input

The Input operator ensures ordered timestamp delivery and provides message buffering for multiple data types. It acts as a gatekeeper to guarantee that messages flow through the system with strictly increasing timestamps.

## Port Types

Creates ports according to the specified configuration:

- `"number"`: NumberData type ports
- `"boolean"`: BooleanData type ports
- `"vector_number"`: VectorNumberData type ports
- `"vector_boolean"`: VectorBooleanData type ports

## Behavior

For each configured port:

- Only forwards messages with increasing timestamps
- Discards messages with timestamps less than or equal to the last sent message
- Messages are forwarded immediately after being received (no buffering)
- Independent timestamp tracking per port

Example usage:

```json
{
  "id": "multi_input",
  "portTypes": ["number", "boolean", "vector_number"]
}
```

This creates an input operator with three ports:

- Port 0: accepts NumberData messages
- Port 1: accepts BooleanData messages
- Port 2: accepts VectorNumberData messages

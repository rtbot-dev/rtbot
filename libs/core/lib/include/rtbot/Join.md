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
    numPorts:
      type: integer
      description: The number of input ports.
      default: 2
      minimum: 2
  required: ["id"]
---

# Join

`Join` operator is used to synchronize two or more incoming message streams into
a single, consistent output.
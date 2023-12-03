---
behavior:
  buffered: true
  throughput: variable
view:
  shape: demultiplexer
  latex:
    template: |
      \arrow
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator.
      examples: ["demul1"]
    numOutputPorts:
      type: integer
      description: The number of output ports through which data will be routed.
      default: 1
      minimum: 1
      examples: [2]
  required: ["id"]
---

# Demultiplexer

A `Demultiplexer` is an operator that routes its incoming data through its
possible output ports. The choice of the output port is controlled by the
messages received through its control ports.

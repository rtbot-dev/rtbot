---
behavior:
  buffered: true
  throughput: variable
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
    numPorts:
      type: integer
      description: The number of possible input ports. Useful if more than one input is taken.
      default: 1
      minimum: 1
  required: ["id"]
---

# Input

The `Input` operator is used to ensure that only messages with increasing timestamp
are sent to the rest of the program. In certain scenarios data received from
the outside world may arrive not time-ordered, or messages with same timestamp could be
received. In such scenarios the `Input` operator will discard invalid messages to ensure
the proper functioning of the operator's pipeline behind it.
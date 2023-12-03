---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \bullet

jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    numPorts:
      type: integer
      description: The number of ports.
      default: 1
      minimum: 1
  required: ["id"]
---

# Output


`Output` operators are used to pull data out of the program.
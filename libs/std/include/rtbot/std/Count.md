---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      Count
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
  required: ["id"]
---

# Count

Counts how many messages have passthrough it.
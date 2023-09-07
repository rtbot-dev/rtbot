---
behavior:
  buffered: false
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      < {{value}}
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    value:
      type: integer
      description: The reference value
  required: ["id", "value"]
---

# LessThan

Emits only the messages received with value less than the number set. Messages that do not
comply with the condition are ignored.
---
behavior:
  buffered: false
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      if = {{value}}
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

# EqualTo

Emits only the messages received with value equal to the number set. Messages that do not comply with the condition are ignored.
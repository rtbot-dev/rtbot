---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
operators:
  MinTracker:
    latex:
      template: |
        \min
  MaxTracker:
    latex:
      template: |
        \max
jsonschemas:
  - type: object
    title: MinTracker
    properties:
      type:
        type: string
        enum: ["MinTracker"]
      id:
        type: string
        description: The id of the operator
    required: ["id"]
  - type: object
    title: MaxTracker
    properties:
      type:
        type: string
        enum: ["MaxTracker"]
      id:
        type: string
        description: The id of the operator
    required: ["id"]
---

# MinMaxTracker

Cumulative running minimum (`MinTracker`) and maximum (`MaxTracker`) operators.

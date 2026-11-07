---
behavior:
  buffered: true
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      \text{TopK}({{k}})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    k:
      type: integer
      description: Number of top entries to retain
      minimum: 1
    score_index:
      type: integer
      description: Index of the score column in the input vector
      minimum: 0
    descending:
      type: string
      description: Sort direction ('true' for descending, 'false' for ascending)
      enum: ["true", "false"]
      default: "true"
  required: ["id", "k", "score_index"]
---

# TopK

Maintains the top-K vector rows ranked by `score_index`.

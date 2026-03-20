---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  operators:
    CompareSyncGT:
      latex:
        template: |
          i1 > i2
    CompareSyncLT:
      latex:
        template: |
          i1 < i2
    CompareSyncGTE:
      latex:
        template: |
          i1 \geq i2
    CompareSyncLTE:
      latex:
        template: |
          i1 \leq i2
    CompareSyncEQ:
      latex:
        template: |
          |i1-i2| \leq {{tolerance}}
    CompareSyncNEQ:
      latex:
        template: |
          |i1-i2| > {{tolerance}}
jsonschemas:
  - type: object
    title: CompareSyncGT
    properties:
      type:
        type: string
        enum: ["CompareSyncGT"]
      id:
        type: string
        description: The id of the operator
      maxSizePerPort:
        type: integer
        minimum: 1
        default: 17280
        description: Maximum number of messages buffered per port
    required: ["id"]
  - type: object
    title: CompareSyncLT
    properties:
      type:
        type: string
        enum: ["CompareSyncLT"]
      id:
        type: string
        description: The id of the operator
      maxSizePerPort:
        type: integer
        minimum: 1
        default: 17280
        description: Maximum number of messages buffered per port
    required: ["id"]
  - type: object
    title: CompareSyncGTE
    properties:
      type:
        type: string
        enum: ["CompareSyncGTE"]
      id:
        type: string
        description: The id of the operator
      maxSizePerPort:
        type: integer
        minimum: 1
        default: 17280
        description: Maximum number of messages buffered per port
    required: ["id"]
  - type: object
    title: CompareSyncLTE
    properties:
      type:
        type: string
        enum: ["CompareSyncLTE"]
      id:
        type: string
        description: The id of the operator
      maxSizePerPort:
        type: integer
        minimum: 1
        default: 17280
        description: Maximum number of messages buffered per port
    required: ["id"]
  - type: object
    title: CompareSyncEQ
    properties:
      type:
        type: string
        enum: ["CompareSyncEQ"]
      id:
        type: string
        description: The id of the operator
      tolerance:
        type: number
        minimum: 0
        default: 0.0
        description: Maximum absolute difference to consider equal
      maxSizePerPort:
        type: integer
        minimum: 1
        default: 17280
        description: Maximum number of messages buffered per port
    required: ["id"]
  - type: object
    title: CompareSyncNEQ
    properties:
      type:
        type: string
        enum: ["CompareSyncNEQ"]
      id:
        type: string
        description: The id of the operator
      tolerance:
        type: number
        minimum: 0
        default: 0.0
        description: Minimum absolute difference to consider not-equal
      maxSizePerPort:
        type: integer
        minimum: 1
        default: 17280
        description: Maximum number of messages buffered per port
    required: ["id"]
---

# CompareSync Operators

A family of operators that synchronize two NumberData streams by timestamp and emit the result of a comparison as BooleanData.

## Common Ports

| Port | Direction | Type | Description |
| --- | --- | --- | --- |
| i1 | data | NumberData | Left-hand side value |
| i2 | data | NumberData | Right-hand side value |
| o1 | output | BooleanData | Comparison result (`i1 OP i2`) |

Pairs of messages are matched by timestamp. A result is emitted only when both i1 and i2 have messages at the same timestamp.

## Available Operators

| Type | Expression | Parameters |
| --- | --- | --- |
| CompareSyncGT | i1 > i2 | none |
| CompareSyncLT | i1 < i2 | none |
| CompareSyncGTE | i1 ≥ i2 | none |
| CompareSyncLTE | i1 ≤ i2 | none |
| CompareSyncEQ | \|i1 − i2\| ≤ tolerance | `tolerance` (default 0.0) |
| CompareSyncNEQ | \|i1 − i2\| > tolerance | `tolerance` (default 0.0) |

---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
operators:
  CompareGT:
    latex:
      template: |
        > {{value}}
  CompareLT:
    latex:
      template: |
        < {{value}}
  CompareGTE:
    latex:
      template: |
        \geq {{value}}
  CompareLTE:
    latex:
      template: |
        \leq {{value}}
  CompareEQ:
    latex:
      template: |
        = {{value}}\pm{{tolerance}}
  CompareNEQ:
    latex:
      template: |
        \neq {{value}}\pm{{tolerance}}
jsonschemas:
  - type: object
    title: CompareGT
    properties:
      type:
        type: string
        enum: ["CompareGT"]
      id:
        type: string
        description: The id of the operator
      value:
        type: number
        description: The threshold value to compare against
        examples: [42.0]
    required: ["id", "value"]
  - type: object
    title: CompareLT
    properties:
      type:
        type: string
        enum: ["CompareLT"]
      id:
        type: string
        description: The id of the operator
      value:
        type: number
        description: The threshold value to compare against
        examples: [42.0]
    required: ["id", "value"]
  - type: object
    title: CompareGTE
    properties:
      type:
        type: string
        enum: ["CompareGTE"]
      id:
        type: string
        description: The id of the operator
      value:
        type: number
        description: The threshold value to compare against
        examples: [42.0]
    required: ["id", "value"]
  - type: object
    title: CompareLTE
    properties:
      type:
        type: string
        enum: ["CompareLTE"]
      id:
        type: string
        description: The id of the operator
      value:
        type: number
        description: The threshold value to compare against
        examples: [42.0]
    required: ["id", "value"]
  - type: object
    title: CompareEQ
    properties:
      type:
        type: string
        enum: ["CompareEQ"]
      id:
        type: string
        description: The id of the operator
      value:
        type: number
        description: The reference value to compare against
        examples: [42.0]
      tolerance:
        type: number
        description: The allowed deviation from the reference value
        examples: [0.0]
    required: ["id", "value"]
  - type: object
    title: CompareNEQ
    properties:
      type:
        type: string
        enum: ["CompareNEQ"]
      id:
        type: string
        description: The id of the operator
      value:
        type: number
        description: The reference value to compare against
        examples: [42.0]
      tolerance:
        type: number
        description: The allowed deviation from the reference value
        examples: [0.0]
    required: ["id", "value"]
---

# CompareScalar

A family of comparison operators that evaluate a numeric input against a threshold value, producing a boolean output.

## Operators

| Type | Condition | Parameters |
| ---- | --------- | ---------- |
| CompareGT | x > value | value |
| CompareLT | x < value | value |
| CompareGTE | x >= value | value |
| CompareLTE | x <= value | value |
| CompareEQ | \|x - value\| <= tolerance | value, tolerance (default 0.0) |
| CompareNEQ | \|x - value\| > tolerance | value, tolerance (default 0.0) |

## Configuration

```json
{ "id": "gt1", "value": 42.0 }
{ "id": "eq1", "value": 10.0, "tolerance": 0.1 }
```

## Ports

- Input Port 0: NumberData
- Output Port 0: BooleanData (true if condition holds, false otherwise)

## Operation

Each input value is evaluated against the condition and a boolean result is emitted with the same timestamp.

| Time | Input | CompareGT (value=5.0) | CompareLT (value=5.0) |
| ---- | ----- | --------------------- | --------------------- |
| 1    | 3.0   | false                 | true                  |
| 2    | 5.0   | false                 | false                 |
| 3    | 7.0   | true                  | false                 |

## Error Handling

- Throws if receiving invalid message types

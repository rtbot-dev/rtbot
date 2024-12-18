---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
operators:
  GreaterThan:
    latex:
      template: |
        > {{value}}
  LessThan:
    latex:
      template: |
        < {{value}}
  Equal:
    latex:
      template: |
        = {{value}}±{{tolerance}}
  NotEqual:
    latex:
      template: |
        ≠ {{value}}±{{tolerance}}
jsonschemas:
  - type: object
    title: GreaterThan
    properties:
      type:
        type: string
        enum: ["GreaterThan"]
      id:
        type: string
        description: The id of the operator
      value:
        type: number
        description: The threshold value to compare against
        examples: [42.0]
    required: ["id", "value"]
  - type: object
    title: LessThan
    properties:
      type:
        type: string
        enum: ["LessThan"]
      id:
        type: string
        description: The id of the operator
      value:
        type: number
        description: The threshold value to compare against
        examples: [42.0]
    required: ["id", "value"]
  - type: object
    title: EqualTo
    properties:
      type:
        type: string
        enum: ["EqualTo"]
      id:
        type: string
        description: The id of the operator
      value:
        type: number
        description: The reference value to compare against
        examples: [42.0]
      tolerance:
        type: number
        description: The allowed deviation from the reference value (absolute)
        examples: [0.1]
    required: ["id", "value"]
  - type: object
    title: NotEqualTo
    properties:
      type:
        type: string
        enum: ["NotEqualTo"]
      id:
        type: string
        description: The id of the operator
      value:
        type: number
        description: The reference value to compare against
        examples: [42.0]
      tolerance:
        type: number
        description: The allowed deviation from the reference value (absolute)
        examples: [0.1]
    required: ["id", "value"]
---

# EqualTo

The EqualTo operator filters messages, only forwarding those whose values equal a specified reference value within a given tolerance.

## Configuration

- `value`: Reference value to compare against
- `tolerance`: (Optional) Allowed deviation from reference value (default: 0.0)

## Port Configuration

Inputs:

- Port 0: Accepts NumberData messages

Outputs:

- Port 0: Emits matching NumberData messages

## Operation

The operator compares each input value against the reference:

- If |input_value - reference_value| ≤ tolerance: Message is forwarded
- Otherwise: Message is dropped

Example sequence showing filtering behavior:

| Time | Input Value | Output Value | Notes                           |
| ---- | ----------- | ------------ | ------------------------------- |
| 1    | 10.0        | 10.0         | Exact match (ref=10.0, tol=0.1) |
| 2    | 9.95        | 9.95         | Within tolerance                |
| 3    | 10.8        | -            | Outside tolerance               |
| 5    | 10.05       | 10.05        | Within tolerance                |
| 6    | 11.0        | -            | Outside tolerance               |

## Error Handling

- Throws if receiving invalid message types
- Negative tolerance values are converted to their absolute value

## Performance

- O(1) processing per message
- No message buffering
- Memory usage independent of message rate

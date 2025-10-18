---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
operators:
  LessThanOrEqualToReplace:
    latex:
      template: |
        <= {{value}}
jsonschemas:
  - type: object
    title: LessThanOrEqualToReplace
    properties:
      type:
        type: string
        enum: ["LessThanOrEqualToReplace"]
      id:
        type: string
        description: The id of the operator
      value:
        type: number
        description: The threshold value to compare against
        examples: [5.0]
      replaceBy:
        type: number
        description: The replacement value
        examples: [0.0]
    required: ["id", "value", "replaceBy"]
---

# LessThanOrEqualToReplace

The LessThanOrEqualToReplace operator replace the values of certain messages and replace those by {replaceBy} if the values of those messages are less than or Equal to the threshold value

## Configuration

- `value`: Reference value to compare against
- `replaceBy`: The value used to replace

## Port Configuration

Inputs:

- Port 0: Accepts NumberData messages

Outputs:

- Port 0: Emits matching NumberData messages

## Operation

The operator compares each input value against the reference:

- If input_value <= reference_value then the input values is replaced by {replaceBy}
- Otherwise: Message is forwarded without changes

## Error Handling

- Throws if receiving invalid message types

## Performance

- O(1) processing per message
- No message buffering

---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      \&\&
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
      examples: ["and1"]
  required: ["id"]
---

# And

The And operator performs logical conjunction on two synchronized boolean input streams. It inherits timing and synchronization behavior from BinaryJoin.

## Ports

Inputs:

- Port 0: Boolean values from first stream
- Port 1: Boolean values from second stream

Output:

- Port 0: Result of logical AND operation

## Operation

For each pair of synchronized messages, the operator:

1. Performs logical AND on the input values
2. Outputs a message with:
   - Same timestamp as inputs
   - Value is the result of `a && b`

Unsynchronized messages are buffered until their matching message arrives or they become obsolete.

## Example

Here's how the And operator processes a sequence of messages:

| Time | Input 0 | Input 1 | Output |
| ---- | ------- | ------- | ------ |
| 1    | true    | true    | true   |
| 2    | true    | -       | -      |
| 3    | false   | false   | false  |
| 4    | -       | true    | -      |
| 5    | true    | true    | true   |

Note:

- At t=2 and t=4, no output is produced due to missing synchronized inputs
- Only synchronized messages produce outputs
- Time gaps between messages are handled naturally by the synchronization mechanism

## Error Handling

- Invalid message types throw std::runtime_error
- Malformed messages throw std::runtime_error

## Performance Characteristics

- Space Complexity: O(n) where n is the maximum time difference between unsynchronized messages
- Time Complexity: O(1) per message processing

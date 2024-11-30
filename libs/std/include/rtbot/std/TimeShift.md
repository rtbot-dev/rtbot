---
behavior:
  buffered: false
  throughput: constant
view:
  shape: circle
  latex:
    template: |
      t {{shift}}
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    shift:
      type: integer
      description: The amount to shift message timestamps (positive or negative)
      examples: [5, -3]
  required: ["id", "shift"]
---

# TimeShift

The TimeShift operator modifies the timestamps of incoming messages by a fixed amount while preserving their values.

## Ports

- Input Port (0): Accepts messages of type NumberData
- Output Port (0): Emits messages of type NumberData with shifted timestamps

## Behavior

- Each incoming message's timestamp is shifted by the configured amount
- Messages that would result in negative timestamps are dropped
- Message values remain unchanged
- No buffering - messages are processed immediately
- Maintains time ordering of messages

## Examples

Given a TimeShift operator with shift = 5:

```
Time  Input    Output
1     10.0     ->  6: 10.0
3     20.0     ->  8: 20.0
6     30.0     -> 11: 30.0
7     40.0     -> 12: 40.0
10    50.0     -> 15: 50.0
```

Given a TimeShift operator with shift = -3:

```
Time  Input    Output
1     10.0     [dropped]
2     20.0     [dropped]
5     30.0     ->  2: 30.0
7     40.0     ->  4: 40.0
10    50.0     ->  7: 50.0
```

## Error Handling

- Throws if receiving incorrect message types
- Silently drops messages that would result in negative timestamps
- No other error conditions

## Performance Characteristics

- O(1) processing per message
- Constant memory usage
- No buffering or state maintenance

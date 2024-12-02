---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
operators:
  SyncGreaterThan:
    latex:
      template: |
        >
  SyncLessThan:
    latex:
      template: |
        <
  SyncEqual:
    latex:
      template: |
        =
  SyncNotEqual:
    latex:
      template: |
        ≠
jsonschemas:
  - type: object
    title: SyncGreaterThan
    properties:
      type:
        type: string
        enum: ["SyncGreaterThan"]
      id:
        type: string
        description: The id of the operator
      value:
        type: number
        description: The threshold value to compare against
    required: ["id", "value"]
  - type: object
    title: SyncLessThan
    properties:
      type:
        type: string
        enum: ["SyncLessThan"]
      id:
        type: string
        description: The id of the operator
      value:
        type: number
        description: The threshold value to compare against
    required: ["id", "value"]
  - type: object
    title: SyncEqual
    properties:
      type:
        type: string
        enum: ["SyncEqual"]
      id:
        type: string
        description: The id of the operator
      value:
        type: number
        description: The value to compare for equality
      epsilon:
        type: number
        description: The allowed deviation from the reference value (absolute)
        default: 1e-10
    required: ["id", "value"]
  - type: object
    title: SyncNotEqual
    properties:
      type:
        type: string
        enum: ["SyncNotEqual"]
      id:
        type: string
        description: The id of the operator
      value:
        type: number
        description: The value to compare for inequality
      epsilon:
        type: number
        description: The allowed deviation from the reference value (absolute)
        default: 1e-10
    required: ["id", "value"]
---

# FilterSyncBinaryOp

A base class for binary operators that synchronize two input streams and apply a filter condition to determine whether to forward messages from the first input.

## Derived Classes

### SyncGreaterThan

Forwards messages from port 0 when their value is greater than the synchronized message on port 1.

### SyncLessThan

Forwards messages from port 0 when their value is less than the synchronized message on port 1.

### SyncEqualTo

Forwards messages from port 0 when their value approximately equals the synchronized message on port 1 (within small epsilon).

## Port Configuration

### Inputs

- Port 0: Primary input stream
- Port 1: Reference input stream for comparison

### Outputs

- Port 0: Filtered messages from input port 0

## Operation

The operator synchronizes messages between its two input ports and applies a filter condition. When messages with matching timestamps arrive, the filter condition is evaluated. If the condition passes, the message from port 0 is forwarded to the output.

### Message Flow Example

```
Time  Port 0  Port 1  Output (SyncGreaterThan)
1     10.0    5.0     10.0    // 10.0 > 5.0, forwards port 0
2     5.0     10.0    -       // 5.0 !> 10.0, no output
3     15.0    -       -       // No sync message, buffers
4     -       12.0    15.0    // 15.0 > 12.0, forwards buffered
5     20.0    20.0    -       // 20.0 !> 20.0, no output
```

## Implementation Details

- Inherits from BinaryJoin for synchronization logic
- Each derived class implements specific filter_condition
- Uses epsilon comparison for floating point equality in SyncEqualTo
- Zero additional state beyond BinaryJoin (no custom serialization needed)
- Type-safe implementation using templates

## Error Handling

The operator will throw exceptions for:

- Invalid message types
- Port configuration errors
- Type mismatches on input ports

## Use Cases

Ideal for:

- Threshold detection across synchronized streams
- Signal comparison with reference values
- Pattern matching in time series
- Conditional forwarding based on synchronized conditions

## Example Usage

```cpp
// Create operator
auto gt = std::make_unique<SyncGreaterThan>("gt1");

// Set up connections
input1->connect(gt.get(), 0);  // Connect to port 0
input2->connect(gt.get(), 1);  // Connect to port 1
gt->connect(output.get());     // Connect output

// Messages only forwarded when port 0 > port 1
```

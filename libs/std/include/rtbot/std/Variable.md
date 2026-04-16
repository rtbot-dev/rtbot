---
behavior:
  buffered: true
  throughput: variable
view:
  shape: circle
  latex:
    template: |
      Var({{default}})
jsonschema:
  type: object
  properties:
    id:
      type: string
      description: The id of the operator
    default_value:
      type: number
      examples:
        - 0.0
      description: Default value to use when no data is available
      default: 0.0
  required: ["id"]
---

# Variable

A stateful operator that holds piece-wise constant values while maintaining causal consistency.

## Ports

- Data Port 0: Receives updates to the variable's value
- Control Port 0: Receives query timestamps
- Output Port 0: Emits queried values

## Operation

The Variable operator maintains a piece-wise constant function based on received data points. When queried through the control port, it returns the most recent value at or before the query time. If no prior value exists, it returns the default value.

### Example

Time-series representation of variable state:

```
Time  |  5  |  10  |  15  |  20  |  25  |
------|-----|------|------|------|------|
Data  |  -  | 100  |  -   | 200  |  -   |
Query | 42* | 100  | 100  | 200  | 200  |
```

- Default value (42) used as no prior data exists

### Key Features

1. Causal Consistency
   - Queries must proceed forward in time
   - Past values remain accessible until overwritten
2. Default Value

   - Used when no data exists before query time
   - Configurable through constructor

3. Piece-wise Constant
   - Value remains constant between data points
   - Changes only at explicit update times

## State Management

The operator maintains:

- Default value
- Initialization flag
- Message buffers for data and control

All state can be serialized and restored for system persistence.

## Error Handling

Throws exceptions for:

- Invalid message types
- Out-of-order query timestamps
- Type mismatches on ports

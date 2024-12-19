# Filter Synchronization Operators

Operators that synchronize n input streams and apply filtering conditions to determine whether to forward messages from the first input.

## Common Properties

- Accept n input ports (minimum 2)
- Produce one output port (0)
- Filter based on first input compared against all others
- Synchronize messages across all input timestamps
- Output messages only when condition passes against all inputs

## Available Operators

### SyncGreaterThan

Forwards messages from port 0 when their value is greater than ALL synchronized values on other ports.

For n inputs:

```
y(t_n) = x₀(t_n) if x₀(t_n) > x₁(t_n) AND x₀(t_n) > x₂(t_n) AND ... AND x₀(t_n) > xₙ₋₁(t_n)
```

### SyncLessThan

Forwards messages from port 0 when their value is less than ALL synchronized values on other ports.

For n inputs:

```
y(t_n) = x₀(t_n) if x₀(t_n) < x₁(t_n) AND x₀(t_n) < x₂(t_n) AND ... AND x₀(t_n) < xₙ₋₁(t_n)
```

### SyncEqual

Forwards messages from port 0 when their value approximately equals ALL synchronized values on other ports (within epsilon).

For n inputs:

```
y(t_n) = x₀(t_n) if |x₀(t_n) - x₁(t_n)| < ε AND |x₀(t_n) - x₂(t_n)| < ε AND ... AND |x₀(t_n) - xₙ₋₁(t_n)| < ε
```

### SyncNotEqual

Forwards messages from port 0 when their value differs from ALL synchronized values on other ports (beyond epsilon).

For n inputs:

```
y(t_n) = x₀(t_n) if |x₀(t_n) - x₁(t_n)| ≥ ε AND |x₀(t_n) - x₂(t_n)| ≥ ε AND ... AND |x₀(t_n) - xₙ₋₁(t_n)| ≥ ε
```

## Example Message Flows

### SyncGreaterThan with 3 inputs:

```
Time | Port 0 | Port 1 | Port 2 | Output
-----|--------|--------|--------|--------
1    | 10.0   | 5.0    | 7.0    | 10.0   # 10 > 5 AND 10 > 7
2    | 8.0    | 5.0    | 9.0    | -      # 8 > 5 BUT NOT 8 > 9
3    | 15.0   | 10.0   | 12.0   | 15.0   # 15 > 10 AND 15 > 12
```

### SyncEqual with 3 inputs (ε = 0.1):

```
Time | Port 0 | Port 1 | Port 2 | Output
-----|--------|--------|--------|--------
1    | 10.0   | 10.05  | 9.95   | 10.0   # All within ε
2    | 8.0    | 8.0    | 8.2    | -      # 8.2 exceeds ε
3    | 15.0   | 14.95  | 14.98  | 15.0   # All within ε
```

## Usage

```cpp
// Create operators
auto gt = make_sync_greater_than("gt1", 3);  // 3 inputs
auto eq = make_sync_equal("eq1", 3, 0.01);   // 3 inputs, ε=0.01

// Example for greater than
gt->receive_data(create_message<NumberData>(1, NumberData{10.0}), 0);
gt->receive_data(create_message<NumberData>(1, NumberData{5.0}), 1);
gt->receive_data(create_message<NumberData>(1, NumberData{7.0}), 2);
gt->execute();  // Outputs 10.0 since 10 > 5 AND 10 > 7
```

## Implementation Notes

1. All operators maintain backward compatibility with binary behavior when used with 2 inputs.
2. Filter condition must pass against ALL other inputs for output to be generated.
3. First input (port 0) is treated as the reference value for all comparisons.
4. Epsilon comparisons in Equal/NotEqual operators ensure consistent floating-point comparisons.
